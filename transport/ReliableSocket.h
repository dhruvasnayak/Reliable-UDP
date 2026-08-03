#pragma once
#include "../common/Endpoint.h"
#include "../common/TransportHeader.h"
#include <map>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

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

struct SendRequest
{
    TransportHeader header;
    std::string data;
    Endpoint endpoint;
};

class ReliableSocket
{
private:
    // Socket and state
    int sockfd;
    ConnectionState state;
    std::mutex stateMutex;

    uint32_t clientISN;
    uint32_t serverISN;

    Connection conn;

    // Threading and synchronization
    std::atomic<bool> threadRunning;
    std::thread recvThread;
    std::thread sendThread;

    // Send queue and synchronization
    std::queue<SendRequest> sendQueue;
    std::mutex sendQueueMutex;
    std::condition_variable sendCV;

    // Receive buffer and synchronization
    std::map<uint32_t, std::string> recvBuffer;
    std::mutex recvBufferMutex;
    std::condition_variable recvCV;

    // ACK pending and synchronization
    std::map<uint32_t, TransportHeader> ackPendingMap;
    std::mutex ackPendingMutex;
    std::condition_variable ackCV;

    // Handshake synchronization
    bool synAckReceived;
    bool ackReceived;
    TransportHeader synAckBuffer;
    Endpoint handshakeEndpoint;
    std::mutex handshakeMutex;
    std::condition_variable handshakeCV;

    // Private helper methods
    void receiveWorker();
    void sendWorker();
    void queueSend(const TransportHeader& header, const std::string& data, Endpoint& endpoint);
    bool waitForAck(uint32_t seqNumber, int timeoutMs);
    bool waitForHandshake(int timeoutMs);
    void updateConnectionState(ConnectionState newState);

public:
    ReliableSocket();
    ~ReliableSocket();

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

