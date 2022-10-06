#include "BCCommunicator.h"
#include "UploadBaseARINC615A.h"
#include "InitializationFileARINC615A.h"

BCCommunicator::BCCommunicator()
{
    std::cout << "Creating BCCommunicator" << std::endl;

    tftpServer = new TFTPServer();
    uploader = new BCUploader();

    tftpServerThread = nullptr;

    tftpServer->setPort(DEFAULT_ARINC615A_TFTP_PORT);

    tftpServer->registerSectionStartedCallback(BCCommunicator::sectionStartedCbk, this);
    tftpServer->registerSectionFinishedCallback(BCCommunicator::sectionFinishedCbk, this);
    tftpServer->registerOpenFileCallback(BCCommunicator::openFileCbk, this);
    tftpServer->registerCloseFileCallback(BCCommunicator::closeFileCbk, this);
}

BCCommunicator::~BCCommunicator()
{
    std::cout << "Destroying BCCommunicator" << std::endl;

    if (tftpServerThread != nullptr && tftpServerThread->joinable())
    {
        tftpServerThread->join();
        tftpServerThread = nullptr;
    }

    delete tftpServerThread;
    delete uploader;
}

void BCCommunicator::setTftpServerPort(int port)
{
    tftpServer->setPort(port);
}

void BCCommunicator::setTftpServerTimeout(int timeout)
{
    tftpServer->setTimeout(timeout);
}

void BCCommunicator::setTftpDataLoaderIp(std::string ip)
{
    uploader->setTftpDataLoaderIp(ip);
}
void BCCommunicator::setTftpDataLoaderPort(int port)
{
    uploader->setTftpDataLoaderPort(port);
}

TftpServerOperationResult BCCommunicator::sectionStartedCbk(
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

TftpServerOperationResult BCCommunicator::sectionFinishedCbk(
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

    BCCommunicator *thiz = (BCCommunicator *)context;
    if (thiz != nullptr)
    {
        thiz->uploader->notifySectionFinished(sectionHandler);
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

TftpServerOperationResult BCCommunicator::openFileCbk(
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

    BCCommunicator *thiz = (BCCommunicator *)context;

    std::string filenameStr(filename);
    if (thiz != nullptr &&
        ((filenameStr.find(UPLOAD_INITIALIZATION_FILE_EXTENSION) != std::string::npos) ||
         (filenameStr.find(UPLOAD_LOAD_UPLOAD_REQUEST_FILE_EXTENSION) != std::string::npos)))
    {
        return thiz->uploader->handleFile(sectionHandler, fd, filename, mode, bufferSize, context);
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
    return TftpServerOperationResult::TFTP_SERVER_OK;
}

TftpServerOperationResult BCCommunicator::closeFileCbk(
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

void BCCommunicator::listen()
{
    tftpServerThread = new std::thread([this]()
                                       { tftpServer->startListening(); });
}

void BCCommunicator::stopListening()
{
    tftpServer->stopListening();
}
