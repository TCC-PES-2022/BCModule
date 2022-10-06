#include "BCCommunicator.h"
#include "BCUploader.h"
#include "InitializationFileARINC615A.h"

#define DEFAULT_HOST "127.0.0.1"

BCUploader::BCUploader()
{
    std::cout << "Creating BCUploader" << std::endl;

    tftpDataLoaderIp = DEFAULT_HOST;
    tftpDataLoaderPort = DEFAULT_ARINC615A_TFTP_PORT;

    uploaders.clear();
    initFileBuffer = nullptr;

    runUploadersRelease = true;
    uploadersReleaseThread = new std::thread(&BCUploader::uploadersRelease, this);
}

BCUploader::~BCUploader()
{
    std::cout << "Destroying BCUploader" << std::endl;

    if (initFileBuffer != nullptr)
    {
        initFileBuffer.reset();
        initFileBuffer = nullptr;
    }

    runUploadersRelease = false;
    if (uploadersReleaseThread != nullptr && uploadersReleaseThread->joinable())
    {
        uploadersReleaseThread->join();
        uploadersReleaseThread = nullptr;
    }

    uploaders.clear();
}

void BCUploader::setTftpDataLoaderIp(std::string ip)
{
    tftpDataLoaderIp = ip;
}

void BCUploader::setTftpDataLoaderPort(int port)
{
    tftpDataLoaderPort = port;
}

TftpServerOperationResult BCUploader::handleFile(ITFTPSection *sectionHandler,
                                                 FILE **fd,
                                                 char *filename,
                                                 char *mode,
                                                 size_t *bufferSize,
                                                 void *context)
{
    BCCommunicator *communicator = (BCCommunicator *)context;

    std::string fileNameStr(filename);
    std::string cleanFileNameStr = fileNameStr.substr(fileNameStr.find_last_of("/") + 1);
    std::string baseFileNameStr = cleanFileNameStr.substr(0, cleanFileNameStr.find_last_of("."));

    std::cout << "Handling Upload File: " << fileNameStr << std::endl;

    if (fileNameStr.find(UPLOAD_INITIALIZATION_FILE_EXTENSION) != std::string::npos)
    {
        std::cout << "- Load Upload Initialization" << std::endl;
        InitializationFileARINC615A initFile(cleanFileNameStr);

            createUploader(baseFileNameStr);
            if (uploaders[baseFileNameStr]->loadUploadInitialization(
                    fd, bufferSize, fileNameStr) == UploadOperationResult::UPLOAD_OPERATION_OK)
            {
                // TODO: Clean authentication flag
                return TftpServerOperationResult::TFTP_SERVER_OK;
            }
            else
            {
                initFile.setOperationAcceptanceStatusCode(INITIALIZATION_UPLOAD_IS_DENIED);
                initFile.setStatusDescription("Internal error.");
            }

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
    else if (fileNameStr.find(UPLOAD_LOAD_UPLOAD_REQUEST_FILE_EXTENSION) !=
             std::string::npos)
    {
        std::cout << "- Load Upload Request Detected" << std::endl;

        createUploader(baseFileNameStr);
        if (uploaders[baseFileNameStr]->loadUploadRequest(
                fd, bufferSize, fileNameStr) != UploadOperationResult::UPLOAD_OPERATION_OK)
        {
            *fd = NULL;
            if (bufferSize != NULL)
            {
                *bufferSize = 0;
            }
            return TftpServerOperationResult::TFTP_SERVER_ERROR;
        }
    }
    else
    {
        return TftpServerOperationResult::TFTP_SERVER_ERROR;
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

void BCUploader::notifySectionFinished(ITFTPSection *sectionHandler)
{
    for (auto it = uploaders.begin(); it != uploaders.end(); ++it)
    {
        it->second->notify(NotifierEventType::NOTIFIER_EVENT_TFTP_SECTION_CLOSED);
    }
}
void BCUploader::createUploader(std::string fileName)
{
    std::lock_guard<std::mutex> lock(uploadersMutex);
    if (uploaders.find(fileName) == uploaders.end())
    {
        uploaders[fileName] = new UploadTargetHardwareARINC615A(tftpDataLoaderIp, tftpDataLoaderPort);
    }
}

void BCUploader::uploadersRelease()
{
    while (runUploadersRelease)
    {
        for (auto it = uploaders.begin(); it != uploaders.end(); ++it)
        {
            std::lock_guard<std::mutex> lock(uploadersMutex);
            UploadTargetHardwareARINC615AState state;
            it->second->getState(state);
            if (state == UploadTargetHardwareARINC615AState::FINISHED ||
                state == UploadTargetHardwareARINC615AState::ERROR)
            {
                std::cout << "Releasing uploader: " << it->first << std::endl;
                delete it->second;
                uploaders.erase(it);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}