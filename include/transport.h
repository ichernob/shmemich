#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PROTOCOL_VERSION 0

typedef enum {
    MSG_REQ_CONNECT,
    MSG_RSP_CONNECT,
    MSG_REQ_DISCONNECT,
    MSG_RSP_DISCONNECT,
    MSG_ERROR
} MessageType;

typedef enum {
    STATUS_OK,           ///< Request was successfully proceed
    STATUS_ERROR,
    STATUS_CLIENTS_LIMIT_EXCEED,
    STATUS_NO_CLIENTS_TO_DISCONNECT,
} MessageStatus;

typedef struct __attribute__((__packed__)) {
    uint8_t version;     ///< Protocol version
    uint8_t type;        ///< Message type
    uint8_t size;        ///< Message payload size
    uint8_t status;      ///< Message status
} Header;

typedef struct {
    Header header;        ///< Message header
} Error;

typedef struct {
    Header header;        ///< Message header
    MessageStatus status; ///< Service status
    char shmempath[128];
    int shmempathlen;
} Status;

class Transport {
  public:
    Transport() {};
    ~Transport() {};

    static Status createReqConnectMsg() {
        Status req;
        req.header.version = PROTOCOL_VERSION;
        req.header.type = MSG_REQ_CONNECT;
        req.header.size = 0;
        req.header.status = STATUS_OK;
        return req;
    }
    static Status createRspConnectMsg(
        Header* request, MessageStatus status
    ) {
        Status resp;
        resp.header.version = PROTOCOL_VERSION;
        resp.header.type = MSG_RSP_CONNECT;
        resp.header.size = 4;
        resp.header.status = STATUS_OK;
        resp.status = status;
        return resp;
    }

    static Status createReqDisconnectMsg() {
        Status req;
        req.header.version = PROTOCOL_VERSION;
        req.header.type = MSG_REQ_DISCONNECT;
        req.header.size = 0;
        req.header.status = STATUS_OK;
        return req;
    }
    static Status createRspDisconnectMsg(
        Header* request, MessageStatus status
    ) {
        Status resp;
        resp.header.version = PROTOCOL_VERSION;
        resp.header.type = MSG_RSP_DISCONNECT;
        resp.header.size = 4;
        resp.header.status = STATUS_OK;
        resp.status = status;
        return resp;
    }

    static Error createErrorMsg(Header* request) {
        Error resp;
        resp.header.version = PROTOCOL_VERSION;
        resp.header.type = MSG_ERROR;
        resp.header.size = 0;
        resp.header.status = STATUS_ERROR;
        return resp;
    }

}; // class Transport
