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

    uploader = nullptr;
    initFileBuffer = nullptr;

    installedLrus.clear();
}

BLUploader::~BLUploader()
{
    std::cout << "Destroying BLUploader" << std::endl;

    if (initFileBuffer != nullptr)
    {
        initFileBuffer.reset();
        initFileBuffer = nullptr;
    }

    if (uploader != nullptr)
    {
        delete uploader->uploader;
        delete uploader;
        uploader = nullptr;
    }

    installedLrus.clear();
}

void BLUploader::setTftpDataLoaderIp(std::string ip)
{
    tftpDataLoaderIp = ip;
}

void BLUploader::setTftpDataLoaderPort(int port)
{
    tftpDataLoaderPort = port;
}
void BLUploader::addLru(LruInfo lruInfo)
{
    installedLrus.push_back(lruInfo);
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

        this->compatibilityFileContent.clear();
        this->receivedImages.clear();

        // TODO: This check should be called by the UploadTargetHardwareARINC615A
        //       implement a callback for it.
        if (communicator->isAuthenticated(baseFileNameStr))
        {
            // TODO: Send WAIT if an uploader is already running
            createUploader(baseFileNameStr);
            if (uploader->uploader->loadUploadInitialization(
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
        //TODO: If initiation fails, the operation will remain authenticated
        //      Add a timeout to clear in case of upload failure before this step.
        communicator->clearAuthentication(baseFileNameStr);

        // std::lock_guard<std::mutex> lock(uploadersMutex);
        std::cout << "- Load Upload Request Detected" << std::endl;

        if (uploader->uploader->loadUploadRequest(
                fd, bufferSize, fileNameStr) != UploadOperationResult::UPLOAD_OPERATION_OK)
        {
            *fd = NULL;
            if (bufferSize != NULL)
            {
                *bufferSize = 0;
            }
            return TftpServerOperationResult::TFTP_SERVER_ERROR;
        }
        uploader->waitingLUR = true;
    }
    else
    {
        return TftpServerOperationResult::TFTP_SERVER_ERROR;
    }

    return TftpServerOperationResult::TFTP_SERVER_OK;
}

void BLUploader::notifySectionFinished(ITFTPSection *sectionHandler)
{
    // std::lock_guard<std::mutex> lock(uploadersMutex);
    if (uploader != nullptr && uploader->waitingLUR)
    {
        uploader->waitingLUR = false;
        uploader->uploader->notify(NotifierEventType::NOTIFIER_EVENT_TFTP_SECTION_CLOSED);
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
    bool integrityCheckOk = true;
    for (auto &file : files)
    {
        std::cout << "Checking file: " << file << std::endl;

        //TODO: Try to load with tinyxml2 to check if is a valid XML
        if (file.find(".xml") == std::string::npos)
        {

            FILE *fp = fopen(file.c_str(), "rb");
            if (fp == NULL)
            {
                std::cout << "File not found: " << file << std::endl;
                checkDescription = checkDescription + "INTEGRITY CHECK ERROR: File not found: " + file;
                checkDescription = checkDescription + "\n";
                integrityCheckOk = false;
                continue;
            }

            fseek(fp, 0, SEEK_SET);
            unsigned char pn[PN_SIZE];
            fread(pn, 1, PN_SIZE, fp);

            std::string pnStr;
            for (int i = 0; i < PN_SIZE; i++)
            {
                // TO HEX STRING
                char hex[3];
                sprintf(hex, "%02X", pn[i]);
                pnStr = pnStr + hex;
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
                integrityCheckOk = false;
                continue;
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
                integrityCheckOk = false;
                continue;
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
                    integrityCheckOk = false;
                    continue;
                }

                if (!pSoftwareNode->FirstChildElement("LRU"))
                {
                    std::cout << "LRU element not found" << std::endl;
                    checkDescription = checkDescription + "INTEGRITY CHECK ERROR: Compatibility file format error";
                    checkDescription = checkDescription + "\n";
                    integrityCheckOk = false;
                    continue;
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
                        integrityCheckOk = false;
                        continue;
                    }

                    thiz->compatibilityFileContent[pn].push_back(std::make_pair(name, lruPn));
                }
            }
        }
    }

    if (!integrityCheckOk)
    {
        std::cout << "Integrity check failed !" << std::endl;
        return UploadOperationResult::UPLOAD_OPERATION_ERROR;
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

    bool compatibilityCheckOk = true;
    for (long unsigned int i = 0; i < thiz->receivedImages.size(); ++i)
    {
        std::string pn = thiz->receivedImages[i];

        // Check if the PN is in the compatibility file
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>::iterator compatibilityIt = thiz->compatibilityFileContent.find(pn);
        if (compatibilityIt == thiz->compatibilityFileContent.end())
        {
            std::cout << "Image not found in compatibility file: " << pn << std::endl;
            checkDescription = checkDescription + "COMPATIBILITY CHECK ERROR: Image not found in compatibility file: " + pn;
            checkDescription = checkDescription + "\n";
            compatibilityCheckOk = false;
            continue;
        }

        // Check if LUR is installed
        std::vector<std::pair<std::string, std::string>> lrus = compatibilityIt->second;
        bool found = false;
        for (std::vector<std::pair<std::string, std::string>>::iterator lrusIt = lrus.begin();
             lrusIt != lrus.end(); ++lrusIt)
        {
            for (std::vector<LruInfo>::iterator installedLursIt = thiz->installedLrus.begin();
                 installedLursIt != thiz->installedLrus.end(); ++installedLursIt)
            {
                // TODO: Is it necessary to check the name too ?
                //  if (installedLursIt->name == lrusIt->first && installedLursIt->pn == lrusIt->second)
                if (installedLursIt->lruPn == lrusIt->second)
                {
                    found = true;
                    std::cout << std::endl;
                    std::cout << "COMPATIBILITY MATCH: " << std::endl;
                    std::cout << "Image: " << pn << std::endl;
                    std::cout << "LRU Name: " << installedLursIt->lruName << std::endl;
                    std::cout << "LRU PN: " << installedLursIt->lruPn << std::endl;
                    std::cout << std::endl;
                    break;
                }
            }
        }
        if (!found)
        {
            std::cout << "No compatible LRU installed for: " << pn << std::endl;
            checkDescription = checkDescription + "COMPATIBILITY CHECK ERROR: No compatible LRU installed for: " + pn;
            checkDescription = checkDescription + "\n";
            compatibilityCheckOk = false;
            continue;
        }
    }

    if (!compatibilityCheckOk)
    {
        std::cout << "Compatibility check failed !" << std::endl;
        return UploadOperationResult::UPLOAD_OPERATION_ERROR;
    }

    std::cout << "All good !" << std::endl;
    return UploadOperationResult::UPLOAD_OPERATION_OK;
}

void BLUploader::createUploader(std::string fileName)
{
    if (uploader != nullptr)
    {
        delete uploader->uploader;
        delete uploader;
    }

    uploader = new UploaderContext();
    uploader->uploader = new UploadTargetHardwareARINC615A(tftpDataLoaderIp, tftpDataLoaderPort);
    uploader->uploader->registerCheckFilesCallback(BLUploader::checkFilesCbk, this);
    uploader->uploader->registerTransmissionCheckCallback(BLUploader::transmissionCheckCbk, this);
}