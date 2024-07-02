#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>//not neede ??
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "pshm_server.h"
#include "transport.h"

using namespace std;

PshmServer *server_ptr = nullptr;

PshmServer::PshmServer(const char *shmempath) : 
    _addrlen(sizeof(_address)), shmempath(shmempath) {}

PshmServer::~PshmServer() {
    quit();
}

void PshmServer::createCommunicationServices() {
    // Create a socket file descriptor
    if ((_serverFd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        release_resources();
        perror_exit("socket failed");
    }
    unlink(_socketPath.c_str());
    // Set the socket address parameters
    _address.sun_family = AF_UNIX;
    strncpy(_address.sun_path, _socketPath.c_str(), sizeof(_address.sun_path) - 1);
    // Bind the socket to the specified address
    if (bind(_serverFd, (struct sockaddr*)&_address, sizeof(_address)) == -1) {
        release_resources();
        perror_exit("bind failed");
    }
    // Set the server socket to non-blocking mode
    if (fcntl(_serverFd, F_SETFL, O_NONBLOCK) < 0) {
        release_resources();
        perror_exit("fcntl failed");
    }
    // Listen for incoming connections
    if (listen(_serverFd, MAX_PENDING_CONNECTIONS) == -1) {
        release_resources();
        perror_exit("listen failed");
    }
}

void PshmServer::destroyCommunicationServices() {
    close(_serverFd);
    unlink(_socketPath.c_str());
}

void PshmServer::processMessage(int clientFd) {
    char buffer[256];
    int numBytes = read(clientFd, buffer, sizeof(buffer));
    cout << "Received " << numBytes << " bytes" << endl;

    if (numBytes > 0) {
        Header* header = reinterpret_cast<Header*>(buffer);
        cout << "\nReceive message:" << endl;
        cout << "\tVersion: " << static_cast<int>(header->version) << endl;
        cout << "\tMessage type: " << static_cast<int>(header->type) << endl;
        cout << "\tMessage size: " << static_cast<int>(header->size) << endl;
        cout << "\tMessage status: " << static_cast<int>(header->status) << endl;

        switch (header->type) {
        case MSG_REQ_CONNECT:
            cout << "Receive REQ_CONNECT message" << endl;
            if (connected_cnt < MAX_PENDING_CONNECTIONS) {
                cout << "\tSending path to shared memory: " << shmempath << " ..." << endl;
                Status resp = Transport::createRspConnectMsg(header, STATUS_OK);
                strcpy(resp.shmempath, shmempath);
                resp.shmempathlen = strlen(shmempath);
                send(clientFd, &resp, sizeof(resp), 0);
                connected_cnt++;
            } else {
                cout << "\tReached limit for available connections, sending back ERROR" << endl;
                Error resp = Transport::createErrorMsg(header);
                resp.header.status = STATUS_CLIENTS_LIMIT_EXCEED;
                send(clientFd, &resp, sizeof(resp), 0);
            }
            break;
        case MSG_REQ_DISCONNECT:
            cout << "Receive REQ_DISCONNECT message" << endl;
            if (connected_cnt > 0) {
                cout << "\tProcessing REQ_DOSCONNECT message" << endl;
                Status resp = Transport::createRspDisconnectMsg(header, STATUS_OK);
                send(clientFd, &resp, sizeof(resp), 0);
                connected_cnt--;
            } else {
                cout << "\tNo any connections, sending back ERROR" << endl;
                Error resp = Transport::createErrorMsg(header);
                resp.header.status = STATUS_NO_CLIENTS_TO_DISCONNECT;
                send(clientFd, &resp, sizeof(resp), 0);
            }
            break;
        default:
            cout << "Receive unsupported message" << endl;
            Error resp = Transport::createErrorMsg(header);
            send(clientFd, &resp, sizeof(resp), 0);
            break;
        }
    }
}

void PshmServer::core() {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // wait no more than 100 ms
    char buffer[BF_SZ];

    if (sem_post(&shm_ptr->sem3) == -1) {//signal to the first any sender.
        release_resources();
        perror_exit("sem_post-mutex-sem");
    }

    while (_loop) {
        fd_set readfds;
        int max_fd, activity;

        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to the set
        FD_SET(_serverFd, &readfds);
        max_fd = _serverFd;

        // Wait for activity on any socket
        activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            cout << "Total ready read descriptors: " << activity << " ..." << endl;
            release_resources();
            perror_exit("select error");
        }

        // If server socket has activity, it's a new connection
        if (FD_ISSET(_serverFd, &readfds)) {
            int clientFd = accept(_serverFd, (struct sockaddr*)&_address, (socklen_t*)&_addrlen);
            if (clientFd == -1) {
                perror_exit("Failed to accept connection");
            }
            cout << "New connection, socket df is " << clientFd << " ..." << endl;
            processMessage(clientFd);
            close(clientFd);
        }
        if (connected_cnt > 0) {
            cout << "  Waiting for sem1 to increment ..." << endl;
            if (sem_wait(&shm_ptr->sem1) == -1) {//wait for signal from sender.
                release_resources();
                perror_exit("sem_wait: spool-signal-sem");
            }
            // cout << "  Processing shared block update ..." << endl;

            // bf_log_ix used by logger to count buffer to take.
            strcpy(buffer, shm_ptr->bfs[shm_ptr->bf_log_ix]);
            (shm_ptr->bf_log_ix)++;
            if (shm_ptr->bf_log_ix == MAX_BFS) {
                shm_ptr->bf_log_ix = 0;
            }

            if (sem_post(&shm_ptr->sem2) == -1) {//signal to any sender.
                release_resources();
                perror_exit("sem_post: buffer-count-sem");
            }
            // cout << "  Incremented sem2 ..." << endl;
            if (write(log_fd, buffer, strlen(buffer)) != strlen(buffer)) {
                release_resources();
                perror_exit("write: logfile");
            }
        }
    }
}

int PshmServer::run() {
    createCommunicationServices();
    cout << "Communication service was successfully created" << endl;
    core();
    destroyCommunicationServices();
    cout << "Server was stoped" << endl;

    return 0;
}

void PshmServer::release_resources() {
    if (prepared) {
        cout << "Unlinking the " << shmempath << " ..." << endl;
        shm_unlink(shmempath);
        cout << "Closing log descriptor " << LOG_FILE << " ..." << endl;
        close(log_fd);
    }
}

int PshmServer::quit()
{
    release_resources();
    cout << "Stopping the Server ..." << endl;
    raise(SIGUSR1);

    return 0;
}

void PshmServer::prepare() {
    log_fd = open(LOG_FILE, O_CREAT | O_WRONLY | O_APPEND | O_SYNC, 0666);
    if (log_fd == -1) {
        perror_exit("flogopen");
    }
    shmem_fd = shm_open(shmempath, O_CREAT | O_EXCL | O_RDWR, 0660);
    if (shmem_fd == -1) {
        perror_exit("shm_open");
    }
    prepared = true;
    if (ftruncate(shmem_fd, sizeof(struct shmblock)) == -1) {
        release_resources();
        perror_exit("ftruncate");
    }

    shm_ptr = (shmblock *) mmap(NULL, sizeof(*shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        release_resources();
        perror_exit("mmap");
    }

    if (sem_init(&shm_ptr->sem1, 1, 0) == -1) {
        release_resources();
        perror_exit("sem_init-sem1");
    }
    if (sem_init(&shm_ptr->sem2, 1, MAX_BFS) == -1) {
        release_resources();
        perror_exit("sem_init-sem2");
    }
    if (sem_init(&shm_ptr->sem3, 1, 0) == -1) {
        release_resources();
        perror_exit("sem_init-sem3");
    }
}

void usage(char *name)
{
    cout << "Usage: " << name << " /shm-path" << endl;
    exit(EXIT_FAILURE);
}

void interrupt_handler(int sign) {
    server_ptr->quit();
    exit(sign);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
    }
    cout << "PID: " << getpid() << endl;

    char *shmempath = argv[1];

    PshmServer server = PshmServer(shmempath);
    server_ptr = &server;
    signal(SIGUSR1, interrupt_handler);
    server.prepare();
    server.run();

    server.quit();
    exit(EXIT_SUCCESS);
}