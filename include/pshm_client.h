#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "transport.h"
#include "pshm_common.h"

class PshmClient {
  private:
    ctrl_buffer *allowed_buffer = NULL;
    char *shmempath = NULL;
    int mmap_offset = -1;
    const char* _tag;
    bool prepared {false};
    struct shmblock *shm_ptr;

    // File descriptor for the client socket
    int _clientFd;
    // Structure to hold the address information for Unix domain socket communication
    struct sockaddr_un _address;
    // Length of the address structure
    int _addrlen;
    // Socket path
    std::string _socketPath { "/tmp/unix.socket" };

    void processMessage(); ///< Process incomming message
    void prepare();
    void core();
    int createCommunicationServices();   ///< Create transport
    void destroyCommunicationServices(); ///< Destroy transport

  public:
    PshmClient() = delete;
    PshmClient(const char *tag);
    ~PshmClient();

    int run();                           ///< Function to start service
    int quit();                          ///< Function to stop service

}; // class PshmClient
