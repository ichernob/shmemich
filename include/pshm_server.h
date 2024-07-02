#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "pshm_common.h"

#define LOG_FILE "/tmp/logich.log"
#define MAX_PENDING_CONNECTIONS MAX_BFS

class PshmServer {
private:
    bool prepared = false;
    int connected_cnt = 0;
    int shmem_fd;
    int log_fd;
    const char *shmempath;
    struct shmblock *shm_ptr;

    // File descriptor for the server socket
    int _serverFd;
    // File descriptor for the client socket
    // int _clientFd;
    // Structure to hold the address information for Unix domain socket communication
    struct sockaddr_un _address;
    // Length of the address structure
    int _addrlen;
    // Socket path
    std::string _socketPath { "/tmp/unix.socket" };
    bool _loop { true };

    void processMessage(int clientFd); ///< Process incomming request
    void release_resources();

public:
    PshmServer() = delete;
    PshmServer(const char *shmempath);
    ~PshmServer();

    void prepare();                      ///< Function to prepare service
    int run();                           ///< Function to start service
    int quit();                          ///< Function to stop service
    void core();                         ///< Core function
    void createCommunicationServices();  ///< Create transport
    void destroyCommunicationServices(); ///< Destroy transport

}; // class PshmServer
