#include "BLCommunicator.h"
#include "BLUploader.h"
#include "InitializationFileARINC615A.h"

#include "tinyxml2.h"
#include "gcrypt.h"

#define DEFAULT_HOST "127.0.0.1"

#define PN_SIZE 4
#define SHA256_SIZE 32

BLUploader::BLUploader()
{
    std::cout << "Creating BLUploader" << std::endl;

    tftpDataLoaderIp = DEFAULT_HOST;
    tftpDataLoaderPort = DEFAULT_ARINC615A_TFTP_PORT;

    uploaders.clear();
    initFileBuffer = nullptr;

    runUploadersRelease = true;
    uploadersReleaseThread = new std::thread(&BLUploader::uploadersRelease, this);
}

BLUploader::~BLUploader()
{
    std::cout << "Destroying BLUploader" << std::endl;

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

void BLUploader::setTftpDataLoaderIp(std::string ip)
{
    tftpDataLoaderIp = ip;
}

void BLUploader::setTftpDataLoaderPort(int port)
{
    tftpDataLoaderPort = port;
}

TftpServerOperationResult BLUploader::handleFile(ITFTPSection *sectionHandler,
                                                 FILE **fd,
                                                 char *filename,
                                                 char *mode,
                                                 size_t *bufferSize,
                                                 void *context)
{
    BLCommunicator *communicator = (BLCommunicator *)context;

    std::string fileNameStr(filename);
    std::string cleanFileNameStr = fileNameStr.substr(fileNameStr.find_last_of("/") + 1);
    std::string baseFileNameStr = cleanFileNameStr.substr(0, cleanFileNameStr.find_last_of("."));

    std::cout << "Handling Upload File: " << fileNameStr << std::endl;

    if (fileNameStr.find(UPLOAD_INITIALIZATION_FILE_EXTENSION) != std::string::npos)
    {
        std::cout << "- Load Upload Initialization" << std::endl;
        InitializationFileARINC615A initFile(cleanFileNameStr);

        // TODO: This check should be called by the UploadTargetHardwareARINC615A
        //       implement a callback for it.
        if (communicator->isAuthenticated())
        {
            createUploader(baseFileNameStr);
            if (uploaders[baseFileNameStr]->loadUploadInitialization(
                    fd, bufferSize, fileNameStr) == UploadOperationResult::UPLOAD_OPERATION_OK)
            {
                // TODO: Clean authentication flag
                return TftpServerOperationResult::TFTP_SERVER_OK;
            }
            else
            {
                std::cout << "*** INTERNAL ERROR ***" << std::endl;
                initFile.setOperationAcceptanceStatusCode(INITIALIZATION_UPLOAD_IS_DENIED);
                initFile.setStatusDescription("Internal error.");
            }
        }
        else
        {
            std::cout << "*** AUTHENTICATION IS REQUIRED ***" << std::endl;
            initFile.setOperationAcceptanceStatusCode(INITIALIZATION_UPLOAD_IS_DENIED);
            initFile.setStatusDescription("Authentication is required.");
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

void BLUploader::notifySectionFinished(ITFTPSection *sectionHandler)
{
    for (auto it = uploaders.begin(); it != uploaders.end(); ++it)
    {
        it->second->notify(NotifierEventType::NOTIFIER_EVENT_TFTP_SECTION_CLOSED);
    }
}

UploadOperationResult BLUploader::checkFilesCbk(
    std::vector<std::string> files,
    std::string &checkDescription,
    void *context)
{
    if (context == NULL)
    {
        checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Internal error";
        checkDescription = checkDescription + "\n";
        return UploadOperationResult::UPLOAD_OPERATION_ERROR;
    }

    BLUploader *thiz = (BLUploader *)context;

    std::cout << "-> Integrity check" << std::endl;
    for (auto &file : files)
    {
        std::cout << "Checking file: " << file << std::endl;

        if (file.find(".xml") == std::string::npos)
        {

            FILE *fp = fopen(file.c_str(), "rb");
            if (fp == NULL)
            {
                std::cout << "File not found: " << file << std::endl;
                checkDescription = checkDescription + "INTEGRITY CHECK ERROR: File not found: " + file;
                checkDescription = checkDescription + "\n";
            }

            fseek(fp, 0, SEEK_SET);
            unsigned char pn[PN_SIZE];
            fread(pn, 1, PN_SIZE, fp);

            std::string pnStr;
            for (int i = 0; i < PN_SIZE; i++)
            {
                pnStr = pnStr + std::to_string(pn[i]);
            }
            thiz->receivedImages.push_back(pnStr);

            // Read SHA256
            char filesha256[SHA256_SIZE + 1];
            fread(filesha256, 1, SHA256_SIZE, fp);
            filesha256[SHA256_SIZE] = '\0';

            // Read data
            fseek(fp, 0, SEEK_END);
            size_t dataSize = ftell(fp) - PN_SIZE - SHA256_SIZE;
            fseek(fp, PN_SIZE + SHA256_SIZE, SEEK_SET);
            char *data = new char[dataSize];
            fread(data, 1, dataSize, fp);

            gcry_md_hd_t hd;
            gcry_md_open(&hd, GCRY_MD_SHA256, 0);
            gcry_md_write(hd, data, dataSize);
            unsigned char *digest = gcry_md_read(hd, GCRY_MD_SHA256);

            if (memcmp(digest, filesha256, SHA256_SIZE) != 0)
            {
                std::cout << "File corrupted: " << file << std::endl;
                checkDescription = checkDescription + "INTEGRITY CHECK ERROR: File corrupted: " + file;
                checkDescription = checkDescription + "\n";
            }
        }
        else
        {
            tinyxml2::XMLDocument doc;
            doc.LoadFile(file.c_str());

            tinyxml2::XMLElement *root = doc.FirstChildElement("COMPATIBILITY");
            if (root == NULL)
            {
                std::cout << "COMPATIBILITY element not found" << std::endl;
                checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Compatibility file format error";
                checkDescription = checkDescription + "\n";
                return UploadOperationResult::UPLOAD_OPERATION_ERROR;
            }

            tinyxml2::XMLElement *pSoftwareNode = root->FirstChildElement("SOFTWARE");
            for (; pSoftwareNode; pSoftwareNode = pSoftwareNode->NextSiblingElement())
            {
                const char *pn = pSoftwareNode->Attribute("PN");
                if (pn == NULL)
                {
                    std::cout << "PN attribute not found" << std::endl;
                    checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Compatibility file format error";
                    checkDescription = checkDescription + "\n";
                    return UploadOperationResult::UPLOAD_OPERATION_ERROR;
                }

                if (!pSoftwareNode->FirstChildElement("LRU"))
                {
                    std::cout << "LRU element not found" << std::endl;
                    checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Compatibility file format error";
                    checkDescription = checkDescription + "\n";
                    return UploadOperationResult::UPLOAD_OPERATION_ERROR;
                }

                tinyxml2::XMLElement *pLruNode = pSoftwareNode->FirstChildElement("LRU");
                for (; pLruNode; pLruNode = pLruNode->NextSiblingElement())
                {
                    const char *name = pLruNode->Attribute("name");
                    const char *lruPn = pLruNode->Attribute("PN");

                    if (name == NULL || lruPn == NULL)
                    {
                        std::cout << "LRU name or PN attribute not found" << std::endl;
                        checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Compatibility file format error";
                        checkDescription = checkDescription + "\n";
                        return UploadOperationResult::UPLOAD_OPERATION_ERROR;
                    }

                    thiz->compatibilityFileContent[pn].push_back(std::make_pair(name, lruPn));
                }
            }
        }
    }

    std::cout << "All good !" << std::endl;
    return UploadOperationResult::UPLOAD_OPERATION_OK;
}

UploadOperationResult BLUploader::transmissionCheckCbk(
    std::string &checkDescription,
    void *context)
{

    if (context == NULL)
    {
        checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Internal error";
        checkDescription = checkDescription + "\n";
        return UploadOperationResult::UPLOAD_OPERATION_ERROR;
    }

    BLUploader *thiz = (BLUploader *)context;

    std::cout << "-> Checking compatibility" << std::endl;

    for (long unsigned int i = 0; i < thiz->receivedImages.size(); ++i)
    {
        std::string pn = thiz->receivedImages[i];
        std::cout << "Checking compatibility with: " << pn << std::endl;
        if (thiz->compatibilityFileContent.find(pn) == thiz->compatibilityFileContent.end())
        {
            std::cout << "Image not found in compatibility file: " << pn << std::endl;
            checkDescription = checkDescription + "COMPATIBILITY CHECK ERROR: Image not found in compatibility file: " + pn;
            checkDescription = checkDescription + "\n";
            return UploadOperationResult::UPLOAD_OPERATION_ERROR;
        }

        long unsigned int compatibilitySize = thiz->compatibilityFileContent[pn].size();
        for (long unsigned int j = 0; j < compatibilitySize; ++j)
        {
            std::cout << "- " << thiz->compatibilityFileContent[pn][j].first
                      << " : " << thiz->compatibilityFileContent[pn][j].second
                      << std::endl;
        }
    }

    std::cout << "All good !" << std::endl;
    return UploadOperationResult::UPLOAD_OPERATION_OK;
}

void BLUploader::createUploader(std::string fileName)
{
    std::lock_guard<std::mutex> lock(uploadersMutex);
    if (uploaders.find(fileName) == uploaders.end())
    {
        uploaders[fileName] = new UploadTargetHardwareARINC615A(tftpDataLoaderIp, tftpDataLoaderPort);
        uploaders[fileName]->registerCheckFilesCallback(BLUploader::checkFilesCbk, this);
        uploaders[fileName]->registerTransmissionCheckCallback(BLUploader::transmissionCheckCbk, this);
    }
}

void BLUploader::uploadersRelease()
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