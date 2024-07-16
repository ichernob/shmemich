#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PROTOCOL_VERSION 0
#define MSG_LENGTH 141

typedef enum : uint8_t {
    MSG_REQ_CONNECT,
    MSG_RSP_CONNECT,
    MSG_REQ_DISCONNECT,
    MSG_RSP_DISCONNECT,
    MSG_ERROR            ///< Error message
} MessageType;
const char* type_map [] = {"MSG_REQ_CONNECT", "MSG_RSP_CONNECT", "MSG_REQ_DISCONNECT", "MSG_RSP_DISCONNECT", "MSG_ERROR"};
std::ostream& operator << (std::ostream& os, const MessageType& mt) {
    return os << type_map[mt];
}

typedef enum : uint8_t {
    STATUS_OK,           ///< Request was successfully proceed
    STATUS_ERROR,        ///< Error happened
    STATUS_CLIENTS_LIMIT_EXCEED,
    STATUS_NO_CLIENTS_TO_DISCONNECT,
} MessageStatus;
const char* status_map[] = {"STATUS_OK", "STATUS_ERROR", "STATUS_CLIENTS_LIMIT_EXCEED", "STATUS_NO_CLIENTS_TO_DISCONNECT"};
std::ostream& operator << (std::ostream& os, const MessageStatus& ms) {
    return os << status_map[ms];
}

typedef struct __attribute__((__packed__)) {
    uint8_t version;      ///< Protocol version
    MessageType type;     ///< Message type
    uint8_t size;         ///< Message payload size
    MessageStatus status; ///< Message status
} Header;
std::ostream& operator << (std::ostream& os, const Header& hdr) {
    return os << "\nReceive message:"
              << "\n\tProtocol Version: " << static_cast<int>(hdr.version)
              << "\n\tMessage type: " << hdr.type
              << "\n\tMessage size: " << static_cast<int>(hdr.size)
              << "\n\tMessage status: " << hdr.status
              << "\n";
}

typedef struct {
    Header header;        ///< Message header
} Error;

union Message {
    char buffer[MSG_LENGTH];  ///< Sum of lengths of nested structures fileds: 128 + 4 +4 + 1 + 1 + 1 + 1 + 1
    struct __attribute__ ((__packed__)) {
        Header header;
        MessageStatus servicestatus;
        char shmempath[128];
        int shmempathlen;
        int mmap_offset;
    } status;
};

class Transport {
  public:
    Transport() {};
    ~Transport() {};

    static Message createReqConnectMsg() {
        Message msg;
        msg.status.header.version = PROTOCOL_VERSION;
        msg.status.header.type = MSG_REQ_CONNECT;
        msg.status.header.size = 0;
        msg.status.header.status = STATUS_OK;
        return msg;
    }
    static Message createRspConnectMsg(MessageStatus status) {
        Message msg;
        msg.status.header.version = PROTOCOL_VERSION;
        msg.status.header.type = MSG_RSP_CONNECT;
        msg.status.header.size = 4;
        msg.status.header.status = STATUS_OK;
        msg.status.servicestatus = status;
        return msg;
    }

    static Message createReqDisconnectMsg() {
        Message req;
        req.status.header.version = PROTOCOL_VERSION;
        req.status.header.type = MSG_REQ_DISCONNECT;
        req.status.header.size = 0;
        req.status.header.status = STATUS_OK;
        return req;
    }
    static Message createRspDisconnectMsg(MessageStatus status) {
        Message rsp;
        rsp.status.header.version = PROTOCOL_VERSION;
        rsp.status.header.type = MSG_RSP_DISCONNECT;
        rsp.status.header.size = 4;
        rsp.status.header.status = STATUS_OK;
        rsp.status.servicestatus = status;
        return rsp;
    }

    static Message createErrorMsg() {
        Message rsp;
        rsp.status.header.version = PROTOCOL_VERSION;
        rsp.status.header.type = MSG_ERROR;
        rsp.status.header.size = 0;
        rsp.status.servicestatus = STATUS_ERROR;
        return rsp;
    }

}; // class Transport
