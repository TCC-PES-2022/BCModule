#ifndef BLUPLOADER_H
#define BLUPLOADER_H

#include <iostream>
#include <thread>
#include <unordered_map>

#include "IFileHandler.h"
#include "BLCommunicator.h"
#include "UploadTargetHardwareARINC615A.h"

class BLUploader : public IFileHandler
{
public:
    BLUploader();
    ~BLUploader();

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);
    void addLru(LruInfo lruInfo);

    TftpServerOperationResult handleFile(ITFTPSection *sectionHandler,
                                         FILE **fd,
                                         char *filename,
                                         char *mode,
                                         size_t *bufferSize,
                                         void *context) override;

    void notifySectionFinished(ITFTPSection *sectionHandler) override;

private:

    class UploaderContext
    {
    public:
        UploaderContext()
        {
            uploader = nullptr;
            waitingLUR = false;
        }
        UploadTargetHardwareARINC615A *uploader;
        bool waitingLUR;
    };
    UploaderContext *uploader;

    static UploadOperationResult checkFilesCbk(
        std::vector<std::string> files,
        std::string &checkDescription,
        void *context);
    static UploadOperationResult transmissionCheckCbk(
        std::string &checkDescription,
        void *context);

    std::vector<std::string> receivedImages;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> compatibilityFileContent;
    std::vector<LruInfo> installedLrus;

    std::shared_ptr<std::vector<uint8_t>> initFileBuffer;

    std::string tftpDataLoaderIp;
    int tftpDataLoaderPort;

    void createUploader(std::string fileName);
    std::mutex uploadersMutex;
    std::thread *uploadersReleaseThread;
};

#endif // BLUPLOADER_H