#ifndef BLUPLOADER_H
#define BLUPLOADER_H

#include <iostream>
#include <thread>
#include <unordered_map>

#include "IFileHandler.h"
#include "UploadTargetHardwareARINC615A.h"

class BLUploader : public IFileHandler
{
public:
    BLUploader();
    ~BLUploader();

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);

    TftpServerOperationResult handleFile(ITFTPSection *sectionHandler,
                                         FILE **fd,
                                         char *filename,
                                         char *mode,
                                         size_t *bufferSize,
                                         void *context) override;

    void notifySectionFinished(ITFTPSection *sectionHandler) override;

private:
    static UploadOperationResult checkFilesCbk(
        std::vector<std::string> files,
        std::string &checkDescription,
        void *context);
    static UploadOperationResult transmissionCheckCbk(
        std::string &checkDescription,
        void *context);

    std::vector<std::string> receivedImages;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> compatibilityFileContent;

    std::shared_ptr<std::vector<uint8_t>> initFileBuffer;

    std::string tftpDataLoaderIp;
    int tftpDataLoaderPort;

    void createUploader(std::string fileName);
    std::mutex uploadersMutex;
    std::unordered_map<std::string, UploadTargetHardwareARINC615A *> uploaders;
    std::thread *uploadersReleaseThread;
    bool runUploadersRelease;
    void uploadersRelease();
};

#endif // BLUPLOADER_H