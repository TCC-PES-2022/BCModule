#ifndef BCUPLOADER_H
#define BCUPLOADER_H

#include <iostream>
#include <thread>

#include "IFileHandler.h"
#include "UploadTargetHardwareARINC615A.h"

class BCUploader : public IFileHandler
{
public:
    BCUploader();
    ~BCUploader();

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

#endif // BCUPLOADER_H