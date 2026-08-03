#pragma once
#include <arpa/inet.h>
#include <string>
#include <cstring>

class Endpoint
{
public:
    sockaddr_in addr;

    Endpoint()
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
    }

    Endpoint(std::string ip, int port)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    }
};

