#ifndef BLAUTHENTICATOR_H
#define BLAUTHENTICATOR_H

#include "gcrypt.h"
#include "IFileHandler.h"
#include "AuthenticationTargetHardware.h"
#include <unordered_map>

class BLAuthenticator : public IFileHandler
{
public:
    BLAuthenticator();
    ~BLAuthenticator();

    void setTftpDataLoaderIp(std::string ip);
    void setTftpDataLoaderPort(int port);

    TftpServerOperationResult handleFile(ITFTPSection *sectionHandler,
                                         FILE **fd,
                                         char *filename,
                                         char *mode,
                                         size_t *bufferSize,
                                         void *context) override;
    void notifySectionFinished(ITFTPSection *sectionHandler) override;

    void clearAuthentication(std::string baseFileName); 
    bool isAuthenticated(std::string baseFileName);

private:
    bool authenticated;
    class AuthenticationContext
    {
    public:
        AuthenticationContext()
        {
            authenticator = nullptr;
            waitingLAR = false;
        }
        AuthenticationTargetHardware *authenticator;
        char *public_key, *private_key;
        bool waitingLAR;
    };
    AuthenticationContext *authenticator;

    static AuthenticationOperationResult checkCertificateCbk(
        unsigned char *fileBuffer,
        size_t fileSize,
        std::string &checkDescription,
        void *context);
    static AuthenticationOperationResult generateCryptographicKeyCbk(
        std::string baseFileName,
        std::vector<uint8_t> &key,
        void *context);

    std::shared_ptr<std::vector<uint8_t>> initFileBuffer;

    std::string tftpDataLoaderIp;
    int tftpDataLoaderPort;

    void createAuthenticator(std::string fileName);
    std::mutex authenticatorsMutex;
};

#endif // BLAUTHENTICATOR_H