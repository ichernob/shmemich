#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#include "pshm_common.h"
#include "pshm_client.h"
#include "transport.h"

using namespace std;

PshmClient::PshmClient(const char *tag) : _addrlen(sizeof(_address)), _tag(tag) {}

PshmClient::~PshmClient() {}

int PshmClient::createCommunicationServices() {
    _address.sun_family = AF_UNIX;
    strncpy(_address.sun_path, _socketPath.c_str(), sizeof(_address.sun_path) - 1);
    if ((_clientFd = socket(AF_UNIX, SOCK_STREAM, 0/*protocol*/)) == -1) {
        cout << "Failed to create socket" << endl;
        return -1;
    }
    if (connect(_clientFd, (struct sockaddr*)&_address, _addrlen) == -1) {
        cout << "Failed to create connection to Server" << endl;
        return -1;
    }
    return 0;
}

void PshmClient::destroyCommunicationServices() {
    close(_clientFd);
}

void PshmClient::processMessage() {
    char buffer[256];
    int numBytes = read(_clientFd, buffer, sizeof(buffer));
    cout << "Received " << numBytes << " bytes" << endl;

    if (numBytes > 0) {
        Message* rsp = reinterpret_cast<Message*>(buffer);
        Header header = rsp->status.header;
        cout << header;

        switch (header.type) {
        case MSG_RSP_CONNECT:
            cout << "Received RSP_CONNECT message: " << static_cast<int>(rsp->status.status) << endl;
            cout << "\tShmempath: " << rsp->status.shmempath << endl;
            cout << "\tLen: " << rsp->status.shmempathlen << endl;
            if (shmempath == NULL) {
                shmempath = (char *) malloc(rsp->status.shmempathlen);
            }
            strncpy(shmempath, rsp->status.shmempath, rsp->status.shmempathlen);
            prepare();
            break;
        case MSG_RSP_DISCONNECT:
            cout << "Received RSP_DISCONNECT message: " << static_cast<int>(rsp->status.status) << endl;
            break;
        case MSG_ERROR:
            cout << "Receive ERROR message" << endl;
            break;
        default:
            cout << "Receive unsupported message" << endl;
            break;
        }
    }
}

int PshmClient::run() {
    cout << "Client was started" << endl;

    int res = createCommunicationServices();
    if (res) {
        cout << "Failed to create communication service" << endl;
        return -1;
    }

    cout << "Communication service was successfully created" << endl;

    Message req = Transport::createReqConnectMsg();
    if (send(_clientFd, &req/*message*/, sizeof(req)/*length*/, 0/*flags*/) == -1) {
        cout << "Failed to send message to Server" << endl;
        return -1;
    }
    processMessage();
    if (prepared) {
        core();
        cout << "Exit from CORE() ..." << endl;
    }
    if (createCommunicationServices() == -1) {
        cout << "Failed to create communication service" << endl;
        return -1;
    }
    cout << "Creating REQ_DISCONNECT ..." << endl;
    Message req_disconnect = Transport::createReqDisconnectMsg();
    if (send(_clientFd, &req_disconnect/*message*/, sizeof(req_disconnect)/*length*/, 0/*flags*/) == -1) {
        cout << "Failed to send message to Server" << endl;
        return -1;
    }
    cout << "Sent REQ_DISCONNECT ..." << endl;
    processMessage();

    destroyCommunicationServices();
    cout << "Client was stoped" << endl;

    return 0;
}

int PshmClient::quit() {
    free(shmempath);
    cout << "Unmapping shared memory ptr" << endl;
    if (munmap(shm_ptr, sizeof(*shm_ptr)) == -1) {
        perror_exit("munmap");
    }
    return 0;
}

void PshmClient::prepare() {
    cout << "Opening " << shmempath << " ..." << endl;
    int shmem_fd = shm_open(shmempath, O_RDWR, 0);
    if (shmem_fd == -1) {
        perror_exit("shm_open");
    }
    cout << "Memory mapping ..." << endl;
    shm_ptr = (shmblock *) mmap(NULL, sizeof(*shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror_exit("mmap");
    }
    prepared = true;
    cout << "Client prepared ..." << endl;
}

void PshmClient::core() {
    char buffer[BF_SZ], *cp;

    cout << "Running core functionality ..." << endl;
    const int EPOCHS = 3;
    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        cout << "    Waiting for sem2 to increment ..." << endl;
        if (sem_wait(&shm_ptr->sem2) == -1) {//wait for signal from logger.
            perror_exit("sem_wait: buffer-count-sem");
        }
        cout << "    Waiting for sem3 to increment ..." << endl;
        if (sem_wait(&shm_ptr->sem3) == -1) {//wait for first signal from logger or for any signal from other senders.
            perror_exit("sem_wait: mutex-sem");
        }

        time_t now = time(NULL);
        cp = ctime(&now);
        int cplen = strlen(cp);
        if (*(cp + cplen - 1) == '\n') {
            *(cp + cplen - 1) = '\0';
        }
        // bf_ix used by senders to avoid data override.
        sprintf(shm_ptr->bfs[shm_ptr->bf_ix], "%s\t%d\t[%s]: Batch %d\n", cp, getpid(), _tag, epoch);
        (shm_ptr->bf_ix)++;//TODO: get rid of indexes. Switch to offsets.
        if (shm_ptr->bf_ix == MAX_BFS) {
            shm_ptr->bf_ix = 0;
        }

        cout << "    Incremented sem3 ..." << endl;
        if (sem_post(&shm_ptr->sem3) == -1) {// signal to another sender.
            perror_exit("sem_post: mutex-sem");
        }
        if (sem_post(&shm_ptr->sem1) == -1) {//signal to logger.
            perror_exit("sem_post: spool_signal_sem");
        }
        cout << "    Incremented sem1 ..." << endl;
        sleep(1);
    }
}

void usage(char *name)
{
    cout << "Usage: " << name << " tag-string" << endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int fd;
    char *shmempath, *tag_string;
    size_t tag_len;
    struct shmblock *shm_ptr;

    if (argc != 2) {
        usage(argv[0]);
    }

    tag_string = argv[1];
    tag_len = strlen(tag_string);

    if (tag_len > (1 << 3)) {
        fprintf(stderr, "Tag string is too long, it should not exceed %d characters.\n", (1 << 3));
        exit(EXIT_FAILURE);
    }

    PshmClient client = PshmClient(tag_string);
    client.run();
    client.quit();

    exit(EXIT_SUCCESS);
}
