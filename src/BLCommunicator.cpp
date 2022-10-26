#include "BLCommunicator.h"
#include "BLUploader.h"
#include "BLAuthenticator.h"
#include "UploadBaseARINC615A.h"
#include "InitializationFileARINC615A.h"
#include "InitializationAuthenticationFile.h"

BLCommunicator::BLCommunicator()
{
    std::cout << "Creating BLCommunicator" << std::endl;

    tftpServer = new TFTPServer();
    uploader = new BLUploader();
    authenticator = new BLAuthenticator();

    tftpServerThread = nullptr;

    wow = false;
    stopped = false;
    maintenanceMode = false;

    tftpServer->setPort(DEFAULT_ARINC615A_TFTP_PORT);

    tftpServer->registerSectionStartedCallback(BLCommunicator::sectionStartedCbk, this);
    tftpServer->registerSectionFinishedCallback(BLCommunicator::sectionFinishedCbk, this);
    tftpServer->registerOpenFileCallback(BLCommunicator::openFileCbk, this);
    tftpServer->registerCloseFileCallback(BLCommunicator::closeFileCbk, this);
}

BLCommunicator::~BLCommunicator()
{
    std::cout << "Destroying BLCommunicator" << std::endl;

    if (tftpServerThread != nullptr && tftpServerThread->joinable())
    {
        tftpServerThread->join();
        tftpServerThread = nullptr;
    }

    delete tftpServerThread;
    delete uploader;
    delete authenticator;
}

void BLCommunicator::setTftpServerPort(int port)
{
    tftpServer->setPort(port);
}

void BLCommunicator::setTftpServerTimeout(int timeout)
{
    tftpServer->setTimeout(timeout);
}

void BLCommunicator::setTftpDataLoaderIp(std::string ip)
{
    uploader->setTftpDataLoaderIp(ip);
    authenticator->setTftpDataLoaderIp(ip);
}
void BLCommunicator::setTftpDataLoaderPort(int port)
{
    uploader->setTftpDataLoaderPort(port);
    authenticator->setTftpDataLoaderPort(port);
}

void BLCommunicator::setWow(bool wow)
{
    this->wow = wow;
}

void BLCommunicator::setMaintenanceMode(bool maintenanceMode)
{
    this->maintenanceMode = maintenanceMode;
}

void BLCommunicator::setStopped(bool stopped)
{
    this->stopped = stopped;
}

void BLCommunicator::addLru(LruInfo lruInfo)
{
    uploader->addLru(lruInfo);
}

bool BLCommunicator::isAuthenticated(std::string baseFileName)
{
    return authenticator->isAuthenticated(baseFileName);
}

void BLCommunicator::clearAuthentication(std::string baseFileName)
{
    return authenticator->clearAuthentication(baseFileName);
}

TftpServerOperationResult BLCommunicator::sectionStartedCbk(
    ITFTPSection *sectionHandler,
    void *context)
{
    TftpSectionId id;
    sectionHandler->getSectionId(&id);
    std::string ip;
    sectionHandler->getClientIp(ip);
    std::cout << "Section started - "
              << "ID: " << id << ", "
              << "IP: " << ip << std::endl;

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

TftpServerOperationResult BLCommunicator::sectionFinishedCbk(
    ITFTPSection *sectionHandler,
    void *context)
{
    TftpSectionId id;
    sectionHandler->getSectionId(&id);
    std::string ip;
    sectionHandler->getClientIp(ip);
    std::cout << "Section finished - "
              << "ID: " << id << ", "
              << "IP: " << ip << std::endl;

    BLCommunicator *thiz = (BLCommunicator *)context;
    if (thiz != nullptr)
    {
        thiz->uploader->notifySectionFinished(sectionHandler);
        thiz->authenticator->notifySectionFinished(sectionHandler);
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

TftpServerOperationResult BLCommunicator::openFileCbk(
    ITFTPSection *sectionHandler,
    FILE **fd,
    char *filename,
    char *mode,
    size_t *bufferSize,
    void *context)
{
    TftpSectionId id;
    sectionHandler->getSectionId(&id);
    std::string ip;
    sectionHandler->getClientIp(ip);
    std::cout << "Open file - "
              << "ID: " << id << ", "
              << "IP: " << ip << ", "
              << "Filename: " << filename << ", "
              << "Mode: " << mode << std::endl;

    BLCommunicator *thiz = (BLCommunicator *)context;

    std::string filenameStr(filename);

    if (thiz->wow && thiz->stopped && thiz->maintenanceMode)
    {
        if (thiz != nullptr &&
            ((filenameStr.find(UPLOAD_INITIALIZATION_FILE_EXTENSION) != std::string::npos) ||
             (filenameStr.find(UPLOAD_LOAD_UPLOAD_REQUEST_FILE_EXTENSION) != std::string::npos)))
        {
            return thiz->uploader->handleFile(sectionHandler, fd, filename, mode, bufferSize, context);
        }
        else if (thiz != nullptr &&
                 ((filenameStr.find(INITIALIZATION_AUTHENTICATION_FILE_EXTENSION) != std::string::npos) ||
                  (filenameStr.find(LOAD_AUTHENTICATION_REQUEST_FILE_EXTENSION) != std::string::npos)))
        {
            return thiz->authenticator->handleFile(sectionHandler, fd, filename, mode, bufferSize, context);
        }
        else
        {
            std::cout << "Operation is not supported." << std::endl;
            InitializationFileARINC615A initFile(filenameStr);
            initFile.setOperationAcceptanceStatusCode(INITIALIZATION_UPLOAD_IS_NOT_SUPPORTED);
            initFile.setStatusDescription("Operation is not supported.");

            std::shared_ptr<std::vector<uint8_t>> fileBuffer = std::make_shared<
                std::vector<uint8_t>>();
            fileBuffer->clear();
            fileBuffer->resize(0);
            initFile.serialize(fileBuffer);

            unsigned char *initFileBuffer = fileBuffer->data();
            FILE *fpInitFile = fmemopen(initFileBuffer, fileBuffer->size(), mode);
            fseek(fpInitFile, 0, SEEK_SET);

            *fd = fpInitFile;
            *bufferSize = fileBuffer->size();
        }
    }
    else
    {
        std::cout << "Operation is denied" << std::endl;
        InitializationFileARINC615A initFile(filenameStr);
        initFile.setOperationAcceptanceStatusCode(INITIALIZATION_UPLOAD_IS_DENIED);
        // initFile.setStatusDescription("Operation is not supported.");

        std::shared_ptr<std::vector<uint8_t>> fileBuffer = std::make_shared<
            std::vector<uint8_t>>();
        fileBuffer->clear();
        fileBuffer->resize(0);
        initFile.serialize(fileBuffer);

        unsigned char *initFileBuffer = fileBuffer->data();
        FILE *fpInitFile = fmemopen(initFileBuffer, fileBuffer->size(), mode);
        fseek(fpInitFile, 0, SEEK_SET);

        *fd = fpInitFile;
        *bufferSize = fileBuffer->size();
    }
    return TftpServerOperationResult::TFTP_SERVER_OK;
}

TftpServerOperationResult BLCommunicator::closeFileCbk(
    ITFTPSection *sectionHandler,
    FILE *fd,
    void *context)
{
    TftpSectionId id;
    sectionHandler->getSectionId(&id);
    std::string ip;
    sectionHandler->getClientIp(ip);
    std::cout << "Close file - "
              << "ID: " << id << ", "
              << "IP: " << ip << std::endl;

    if (fd != NULL)
    {
        fclose(fd);
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

void BLCommunicator::listen()
{
    tftpServerThread = new std::thread([this]()
                                       { tftpServer->startListening(); });
}

void BLCommunicator::stopListening()
{
    tftpServer->stopListening();
}
