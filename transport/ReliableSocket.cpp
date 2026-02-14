#include "ReliableSocket.h"

ReliableSocket::ReliableSocket() {}

bool ReliableSocket::bind(int port) {
    return udp.bindSocket(port);
}

bool ReliableSocket::send(const std::string& data, Endpoint& endpoint) {
    return udp.sendTo(data.c_str(), data.size(), endpoint);
}

std::string ReliableSocket::recv(Endpoint& endpoint) {

    char buffer[1024] = {0};

    int len = udp.recvFrom(buffer, 1024, endpoint);

    if(len > 0)
        return std::string(buffer, len);

    return "";
}
