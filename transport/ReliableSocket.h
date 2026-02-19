#pragma once
#include "../common/Endpoint.h"
#include "../common/TransportHeader.h"
#include <map>
#include <string>

enum ConnectionState
{
    CLOSED,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED
};

struct Connection
{
    uint32_t sendSeq;
    uint32_t expectedSeq;

    std::map<uint32_t, std::string> sendBuffer;
    std::map<uint32_t, std::string> recvBuffer;
};

class ReliableSocket
{
private:
    int sockfd;
    ConnectionState state;

    uint32_t clientISN;
    uint32_t serverISN;

    Connection conn;

public:
    ReliableSocket();

    bool bindSocket(int port);
    bool connect(Endpoint& server);
    bool accept(Endpoint& client);

    bool reliableSend(const std::string& data, Endpoint& endpoint);
    std::string reliableRecv(Endpoint& endpoint);

    bool sendPacket(const TransportHeader& header,
                    const std::string& data,
                    Endpoint& endpoint);

    bool receivePacket(TransportHeader& header,
                       std::string& data,
                       Endpoint& endpoint);
};

