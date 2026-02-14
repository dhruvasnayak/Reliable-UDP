#pragma once
#include "../core/UdpSocket.h"
#include <string>

class ReliableSocket {
public:
    ReliableSocket();

    bool bind(int port);
    bool send(const std::string& data, Endpoint& endpoint);
    std::string recv(Endpoint& endpoint);

private:
    UdpSocket udp;
};
