#ifndef BLCOMMUNICATOR_H
#define BLCOMMUNICATOR_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "TFTPServer.h"
#include "BLUploader.h"
#include "BLAuthenticator.h"

class BLCommunicator
{
public:
    BLCommunicator();
    ~BLCommunicator();

    void setTftpServerPort(int port);
    void setTftpServerTimeout(int timeout);

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);

    bool isAuthenticated();

    void listen();
    void stopListening();

private:
    BLAuthenticator *authenticator;
    BLUploader *uploader;
    TFTPServer *tftpServer;
    std::thread *tftpServerThread;

    static TftpServerOperationResult sectionStartedCbk(
        ITFTPSection *sectionHandler,
        void *context);

    static TftpServerOperationResult sectionFinishedCbk(
        ITFTPSection *sectionHandler,
        void *context);

    static TftpServerOperationResult openFileCbk(
        ITFTPSection *sectionHandler,
        FILE **fd,
        char *filename,
        char *mode,
        size_t *bufferSize,
        void *context);

    static TftpServerOperationResult closeFileCbk(
        ITFTPSection *sectionHandler,
        FILE *fd,
        void *context);
};

#endif // BLCOMMUNICATOR_H