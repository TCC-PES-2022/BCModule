#include <iostream>
#include <csignal>

#include <cjson/cJSON.h>

#include "BCCommunicator.h"

#define CONFIG_FILE "bcconfig.json"

BCCommunicator *communicator;
bool stopBCModule = false;

void signal_handler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    stopBCModule = true;
}

void parse_configuration_file()
{
    std::cout << "Parsing configuration" << std::endl;

    FILE *configFile = fopen(CONFIG_FILE, "r");
    if (configFile == NULL)
    {
        std::cout << "Error opening configuration file" << std::endl;
        exit(1);
    }

    fseek(configFile, 0, SEEK_END);
    size_t configFileSize = ftell(configFile);
    rewind(configFile);

    char *configFileContent = (char *)malloc(configFileSize + 1);
    if (configFileContent == NULL)
    {
        std::cout << "Error allocating memory for configuration file" << std::endl;
        exit(1);
    }

    size_t result = fread(configFileContent, 1, configFileSize, configFile);
    if (result != configFileSize)
    {
        std::cout << "Error reading configuration file" << std::endl;
        exit(1);
    }

    configFileContent[configFileSize] = '\0';

    fclose(configFile);

    cJSON *config = cJSON_Parse(configFileContent);
    if (config == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpTargetHardwareServer = cJSON_GetObjectItemCaseSensitive(
        config, "tftpTargetHardwareServer");
    if (tftpTargetHardwareServer == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpTargetHardwareServerPort = cJSON_GetObjectItemCaseSensitive(
        tftpTargetHardwareServer, "port");
    if (tftpTargetHardwareServerPort == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpTargetHardwareServerTimeout = cJSON_GetObjectItemCaseSensitive(
        tftpTargetHardwareServer, "timeout");
    if (tftpTargetHardwareServerTimeout == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpDataLoaderServer = cJSON_GetObjectItemCaseSensitive(
        config, "tftpDataLoaderServer");
    if (tftpDataLoaderServer == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpDataLoaderServerIp = cJSON_GetObjectItemCaseSensitive(
        tftpDataLoaderServer, "ip");
    if (tftpDataLoaderServerIp == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    cJSON *tftpDataLoaderServerPort = cJSON_GetObjectItemCaseSensitive(
        tftpDataLoaderServer, "port");
    if (tftpDataLoaderServerPort == NULL)
    {
        std::cout << "Error parsing configuration file" << std::endl;
        exit(1);
    }

    communicator->setTftpServerPort(tftpTargetHardwareServerPort->valueint);
    communicator->setTftpServerTimeout(tftpTargetHardwareServerTimeout->valueint);
    communicator->setTftpDataLoaderIp(tftpDataLoaderServerIp->valuestring);
    communicator->setTftpDataLoaderPort(tftpDataLoaderServerPort->valueint);

    std::cout << "###############################################" << std::endl;
    std::cout << "Configuration parsed:" << std::endl;
    std::cout << "TFTP TargetHardware server port: " << tftpTargetHardwareServerPort->valueint << std::endl;
    std::cout << "TFTP server timeout: " << tftpTargetHardwareServerTimeout->valueint << std::endl;
    std::cout << "TFTP DataLoader server IP: " << tftpDataLoaderServerIp->valuestring << std::endl;
    std::cout << "TFTP DataLoader server port: " << tftpDataLoaderServerPort->valueint << std::endl;
    std::cout << "###############################################" << std::endl;
}

void register_signal_handlers()
{
    std::cout << "Registering signal handlers" << std::endl;
    signal(SIGINT, signal_handler);
}

int main(int argc, char const *argv[])
{
    std::cout << "###############################################" << std::endl;
    std::cout << "################## B/C Module #################" << std::endl;
    std::cout << "###############################################" << std::endl;

    communicator = new BCCommunicator();

    parse_configuration_file();
    register_signal_handlers();

    communicator->listen();

    while (!stopBCModule)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    communicator->stopListening();
    delete communicator;

    return 0;
}
