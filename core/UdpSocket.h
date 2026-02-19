#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common/Endpoint.h"

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    bool bindSocket(int port);
    bool sendTo(const char* data, int len, Endpoint& endpoint);
    int recvFrom(char* buffer, int len, Endpoint& endpoint);

private:
    int sock;
};
