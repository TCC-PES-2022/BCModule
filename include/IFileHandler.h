#ifndef IFILEHANDLER_H
#define IFILEHANDLER_H

#include "ITFTPServer.h"

class IFileHandler
{
public:
    virtual ~IFileHandler() {}
    virtual TftpServerOperationResult handleFile(ITFTPSection *sectionHandler,
                                                 FILE **fd,
                                                 char *filename,
                                                 char *mode,
                                                 size_t *bufferSize,
                                                 void *context) = 0;

    virtual void notifySectionFinished(ITFTPSection *sectionHandler) = 0;
};

#endif // IFILEHANDLER_H