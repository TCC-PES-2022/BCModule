
#include <sstream>
#include <fstream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "BLAuthenticator.h"

#include "BLCommunicator.h"
#include "BLAuthenticator.h"
#include "InitializationAuthenticationFile.h"

#include "tinyxml2.h"

#define DEFAULT_HOST "127.0.0.1"

#define PN_SIZE 4
#define SHA256_SIZE 32

// If you change here, remember to change on CommunicationManager
// (AuthenticationManager) as well.
#define KEY_SIZE 2048
#define DATA_SIZE_FIELD_SIZE 4

BLAuthenticator::BLAuthenticator()
{
    std::cout << "Creating BLAuthenticator" << std::endl;

    tftpDataLoaderIp = DEFAULT_HOST;
    tftpDataLoaderPort = DEFAULT_ARINC615A_TFTP_PORT;

    initFileBuffer = nullptr;

    authenticated = false;

    //TODO: This should not be here in a release version
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
}

BLAuthenticator::~BLAuthenticator()
{
    std::cout << "Destroying BLAuthenticator" << std::endl;

    if (initFileBuffer != nullptr)
    {
        initFileBuffer.reset();
        initFileBuffer = nullptr;
    }

    if (authenticator != nullptr)
    {
        delete authenticator->authenticator;
        delete authenticator;
        authenticator = nullptr;
    }
}

void BLAuthenticator::setTftpDataLoaderIp(std::string ip)
{
    tftpDataLoaderIp = ip;
}

void BLAuthenticator::setTftpDataLoaderPort(int port)
{
    tftpDataLoaderPort = port;
}

void BLAuthenticator::clearAuthentication(std::string baseFileName)
{
    authenticated = false;
}

bool BLAuthenticator::isAuthenticated(std::string baseFileName)
{
    return authenticated;
}

TftpServerOperationResult BLAuthenticator::handleFile(ITFTPSection *sectionHandler,
                                                      FILE **fd,
                                                      char *filename,
                                                      char *mode,
                                                      size_t *bufferSize,
                                                      void *context)
{
    std::string fileNameStr(filename);
    std::string cleanFileNameStr = fileNameStr.substr(fileNameStr.find_last_of("/") + 1);
    std::string baseFileNameStr = cleanFileNameStr.substr(0, cleanFileNameStr.find_last_of("."));

    std::cout << "Handling Authentication File: " << fileNameStr << std::endl;

    if (fileNameStr.find(INITIALIZATION_AUTHENTICATION_FILE_EXTENSION) != std::string::npos)
    {
        std::lock_guard<std::mutex> lock(authenticatorsMutex);
        std::cout << "- Load Authentication Initialization << " << baseFileNameStr << std::endl;

        //TODO: Send WAIT if an authenticator is already running
        createAuthenticator(baseFileNameStr);
        if (authenticator->authenticator->loadAuthenticationInitialization(
                fd, bufferSize, fileNameStr) == AuthenticationOperationResult::AUTHENTICATION_OPERATION_OK)
        {
            std::cout << "- Load Authentication Initialization OK" << std::endl;
            return TftpServerOperationResult::TFTP_SERVER_OK;
        }
        else
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;

            InitializationAuthenticationFile initFile(cleanFileNameStr);
            initFile.setOperationAcceptanceStatusCode(OPERATION_IS_DENIED);
            initFile.setStatusDescription("Internal error.");

            if (initFileBuffer == nullptr)
            {
                initFileBuffer = std::make_shared<std::vector<uint8_t>>();
            }
            initFileBuffer->clear();
            initFileBuffer->resize(0);
            initFile.serialize(initFileBuffer);

            std::string serializedJSON;
            initFile.serializeJSON(serializedJSON);

            FILE *fpInitFile = fmemopen(initFileBuffer->data(),
                                        initFileBuffer->size(), mode);
            fseek(fpInitFile, 0, SEEK_SET);

            *fd = fpInitFile;
            if (bufferSize != NULL)
            {
                *bufferSize = initFileBuffer->size();
            }
        }
    }
    else if (fileNameStr.find(LOAD_AUTHENTICATION_REQUEST_FILE_EXTENSION) !=
             std::string::npos)
    {
        std::lock_guard<std::mutex> lock(authenticatorsMutex);
        std::cout << "- Load Authentication Request Detected << " << baseFileNameStr << std::endl;

        if (authenticator->authenticator->loadAuthenticationRequest(
                fd, bufferSize, fileNameStr) != AuthenticationOperationResult::AUTHENTICATION_OPERATION_OK)
        {
            std::cout << "*** INTERNAL ERROR - LAR ***" << std::endl;
            *fd = NULL;
            if (bufferSize != NULL)
            {
                *bufferSize = 0;
            }
            return TftpServerOperationResult::TFTP_SERVER_ERROR;
        }
        authenticator->waitingLAR = true;
    }
    else
    {
        return TftpServerOperationResult::TFTP_SERVER_ERROR;
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

void BLAuthenticator::notifySectionFinished(ITFTPSection *sectionHandler)
{
    // std::lock_guard<std::mutex> lock(authenticatorsMutex);
    if (authenticator != nullptr && authenticator->waitingLAR)
    {
        authenticator->waitingLAR = false;
        authenticator->authenticator->notify(NotifierAuthenticationEventType::NOTIFIER_AUTHENTICATION_EVENT_TFTP_SECTION_CLOSED);
    }
}

AuthenticationOperationResult BLAuthenticator::checkCertificateCbk(
    unsigned char *fileBuffer,
    size_t fileSize,
    std::string &checkDescription,
    void *context)
{
    std::cout << "Checking Certificate" << std::endl;

    if (context == NULL)
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        checkDescription = "Internal error.";
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }
    BLAuthenticator *thiz = (BLAuthenticator *)context;

    std::string asciiCertificate;

    // We receive the certificate splitted, so we need to concatenate the parts
    unsigned char *auxFileBufferPtr = fileBuffer;
    size_t chunkSize = 0;
    for (size_t i = 0; i < DATA_SIZE_FIELD_SIZE; i++, auxFileBufferPtr++)
    {
        chunkSize = (chunkSize << 8) | *auxFileBufferPtr;
    }

    while (chunkSize > 0)
    {
        // Now let's check if the certificate size is correct.
        char *cypheredChunkCertificate = (char *)malloc(chunkSize);

        if (cypheredChunkCertificate == NULL)
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;
            checkDescription = "Internal error.";
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }

        // We receive the certificate in bytes, let's transform to S-Expression.
        for (size_t i = 0; i < chunkSize; i++, auxFileBufferPtr++)
        {
            cypheredChunkCertificate[i] = *auxFileBufferPtr;
        }

        gcry_error_t error;
        gcry_sexp_t data;
        size_t len = strlen(cypheredChunkCertificate);
        if ((error = gcry_sexp_new(&data, cypheredChunkCertificate, len, 1)))
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;
            checkDescription = "Internal error.";
            // printf("Error in sexp_new(%s): %s\nSource: %s\n", cypheredChunkCertificate, gcry_strerror(error), gcry_strsource(error));
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }

        gcry_sexp_t private_sexp;
        len = strlen(thiz->authenticator->private_key);
        if ((error = gcry_sexp_new(&private_sexp, thiz->authenticator->private_key, len, 1)))
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;
            checkDescription = "Internal error.";
            // printf("Error in sexp_new(%s): %s\nSource: %s\n", thiz->authenticator->private_key, gcry_strerror(error), gcry_strsource(error));
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }

        gcry_sexp_t r_plain;
        if ((error = gcry_pk_decrypt(&r_plain, data, private_sexp)))
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;
            checkDescription = "Internal error.";
            // printf("Error in gcry_pk_decrypt(): %s\nSource: %s\n", gcry_strerror(error), gcry_strsource(error));
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }

        gcry_mpi_t r_mpi = gcry_sexp_nth_mpi(r_plain, 0, GCRYMPI_FMT_USG);

        unsigned char *plaintext;
        size_t plaintext_size;
        if ((error = gcry_mpi_aprint(GCRYMPI_FMT_HEX, &plaintext, &plaintext_size, r_mpi)))
        {
            std::cout << "*** INTERNAL ERROR ***" << std::endl;
            checkDescription = "Internal error.";
            // printf("Error in gcry_mpi_aprint(): %s\nSource: %s\n", gcry_strerror(error), gcry_strsource(error));
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }

        // HEX to ascii
        char *ascii = (char *)malloc((plaintext_size / 2) + 1);
        memset(ascii, '\0', (plaintext_size / 2) + 1);
        for (size_t i = 0; i < (plaintext_size / 2); i++)
        {
            char hex[3];
            hex[0] = plaintext[i * 2];
            hex[1] = plaintext[i * 2 + 1];
            hex[2] = '\0';
            ascii[i] = (char)strtol(hex, NULL, 16);
        }

        asciiCertificate += ascii;
        free(cypheredChunkCertificate);
        cypheredChunkCertificate = NULL;

        chunkSize = 0;
        for (size_t i = 0; i < DATA_SIZE_FIELD_SIZE; i++, auxFileBufferPtr++)
        {
            chunkSize = (chunkSize << 8) | *auxFileBufferPtr;
        }
    }

    pid_t openssl_pid = fork();
    if (openssl_pid == 0)
    {
        std::ofstream certificateFile;
        certificateFile.open("/tmp/certificate.crt");
        if (!certificateFile.is_open())
        {
            printf("Error opening certificate file\n");
            exit(1);
        }
        certificateFile << asciiCertificate;
        certificateFile.close();

        const std::vector<std::string> cmdline{"openssl", "verify", "-verbose", "-CAfile", "pes.pem", "/tmp/certificate.crt"};
        std::vector<const char *> argv;
        for (const auto &s : cmdline)
        {
            argv.push_back(s.data());
        }
        argv.push_back(NULL);
        argv.shrink_to_fit();
        errno = 0;

        execvp("openssl", const_cast<char *const *>(argv.data()));
        std::cout << "-> Could not check certificate: OPENSSL ERROR" << std::endl;
        exit(1);
    }
    else if (openssl_pid > 0)
    {
        int status;
        waitpid(openssl_pid, &status, 0);
        if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) == 0)
            {
                std::cout << " *** Certificate is valid ***" << std::endl;
                thiz->authenticated = true;
                return AuthenticationOperationResult::AUTHENTICATION_OPERATION_OK;
            }
            else
            {
                std::cout << " *** Certificate is invalid ***" << std::endl;
                checkDescription = "Certificate is invalid";
                return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
            }
        }
        else
        {
            std::cout << " *** Certificate is invalid ***" << std::endl;
            checkDescription = "Certificate is invalid";
            return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
        }
    }
    else
    {
        std::cout << " *** Error in fork() ***" << std::endl;
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }

    std::cout << "All good!" << std::endl;
    return AuthenticationOperationResult::AUTHENTICATION_OPERATION_OK;
}

AuthenticationOperationResult BLAuthenticator::generateCryptographicKeyCbk(
    std::string baseFileName,
    std::vector<uint8_t> &key,
    void *context)
{

    std::cout << "Generating cryptographic keys" << std::endl;

    if (context == NULL)
    {
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }
    BLAuthenticator *thiz = (BLAuthenticator *)context;

    gcry_error_t error;

    gcry_sexp_t params;
    std::string params_str("(genkey (rsa (transient-key) (nbits 4:" + std::to_string(KEY_SIZE) + ")))");
    printf("Params: %s\n", params_str.c_str());
    if ((error = gcry_sexp_new(&params, params_str.c_str(), params_str.size(), 1)))
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        // printf("Error in sexp_new(%s): %s\nSource: %s\n", params_str.c_str(), gcry_strerror(error), gcry_strsource(error));
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }

    gcry_sexp_t r_key;
    if ((error = gcry_pk_genkey(&r_key, params)))
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        // printf("Error in gcry_pk_genkey(): %s\nSource: %s\n", gcry_strerror(error), gcry_strsource(error));
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }

    gcry_sexp_t public_sexp = gcry_sexp_nth(r_key, 1);
    gcry_sexp_t private_sexp = gcry_sexp_nth(r_key, 2);

    size_t public_buf_len = gcry_sexp_sprint(public_sexp, GCRYSEXP_FMT_ADVANCED, NULL, 0);
    size_t private_buf_len = gcry_sexp_sprint(private_sexp, GCRYSEXP_FMT_ADVANCED, NULL, 0);

    thiz->authenticator->public_key = (char *)gcry_malloc(public_buf_len);
    thiz->authenticator->private_key = (char *)gcry_malloc(private_buf_len);

    if (thiz->authenticator->public_key == NULL ||
        thiz->authenticator->private_key == NULL)
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        // printf("gcry_malloc(%ld) returned NULL in sexp_string()!\n", public_buf_len);
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }
    if (0 == gcry_sexp_sprint(public_sexp, GCRYSEXP_FMT_ADVANCED,
                              thiz->authenticator->public_key, public_buf_len))
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        // printf("Error in gcry_sexp_sprint() for public key\n");
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }
    if (0 == gcry_sexp_sprint(private_sexp, GCRYSEXP_FMT_ADVANCED,
                              thiz->authenticator->private_key, private_buf_len))
    {
        std::cout << "*** INTERNAL ERROR ***" << std::endl;
        // printf("Error in gcry_sexp_sprint() for private key\n");
        return AuthenticationOperationResult::AUTHENTICATION_OPERATION_ERROR;
    }

    // printf("Public Key: %s\n", thiz->authenticator->public_key);
    // printf("Private Key: %s\n", thiz->authenticator->private_key);

    // TODO: If possible, send only the key, not the whole S-Expression
    key = std::vector<uint8_t>(thiz->authenticator->public_key,
                               thiz->authenticator->public_key + public_buf_len);

    return AuthenticationOperationResult::AUTHENTICATION_OPERATION_OK;
}

void BLAuthenticator::createAuthenticator(std::string fileName)
{

    if (authenticator != nullptr)
    {
        delete authenticator->authenticator;
        delete authenticator;
    }

    printf("Creating authenticator for %s\n", fileName.c_str());
    authenticator = new AuthenticationContext();
    authenticator->waitingLAR = false;
    authenticator->authenticator =
        new AuthenticationTargetHardware(tftpDataLoaderIp, tftpDataLoaderPort);
    authenticator->authenticator->registerCheckCertificateCallback(
        BLAuthenticator::checkCertificateCbk, this);
    authenticator->authenticator->registerGenerateCryptographicKeyCallback(
        BLAuthenticator::generateCryptographicKeyCbk, this);
}