#ifndef BLCOMMUNICATOR_H
#define BLCOMMUNICATOR_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "TFTPServer.h"

class BLUploader;
class BLAuthenticator;

typedef struct {
    std::string lruName;
    std::string lruPn;
} LruInfo;

class BLCommunicator
{
public:
    BLCommunicator();
    ~BLCommunicator();

    void setTftpServerPort(int port);
    void setTftpServerTimeout(int timeout);

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);

    void setWow(bool wow);
    void setMaintenanceMode(bool maintenanceMode);
    void setStopped(bool stopped);

    void addLru(LruInfo lruInfo);

    void clearAuthentication(std::string baseFileName);
    bool isAuthenticated(std::string baseFileName);

    void listen();
    void stopListening();

private:
    BLAuthenticator *authenticator;
    BLUploader *uploader;
    TFTPServer *tftpServer;
    std::thread *tftpServerThread;

    bool wow;
    bool stopped;
    bool maintenanceMode;

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