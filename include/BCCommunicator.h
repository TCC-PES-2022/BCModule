#ifndef BCCOMMUNICATOR_H
#define BCCOMMUNICATOR_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "TFTPServer.h"
#include "BCUploader.h"
#include "BCAuthenticator.h"

class BCCommunicator
{
public:
    BCCommunicator();
    ~BCCommunicator();

    void setTftpServerPort(int port);
    void setTftpServerTimeout(int timeout);

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);

    bool isAuthenticated();

    void listen();
    void stopListening();

private:
    BCAuthenticator *authenticator;
    BCUploader *uploader;
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

#endif // BCCOMMUNICATOR_H