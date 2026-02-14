#include "UdpSocket.h"
#include <cstring>

UdpSocket::UdpSocket() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
}

UdpSocket::~UdpSocket() {
    close(sock);
}

bool UdpSocket::bindSocket(int port) {

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    return bind(sock,
                (sockaddr*)&server,
                sizeof(server)) != -1;
}

bool UdpSocket::sendTo(const char* data, int len, Endpoint& endpoint) {

    return sendto(sock,
                  data,
                  len,
                  0,
                  (sockaddr*)&endpoint.addr,
                  sizeof(endpoint.addr)) != -1;
}

int UdpSocket::recvFrom(char* buffer, int len, Endpoint& endpoint) {

    socklen_t addrlen = sizeof(endpoint.addr);

    return recvfrom(sock,
                    buffer,
                    len,
                    0,
                    (sockaddr*)&endpoint.addr,
                    &addrlen);
}
