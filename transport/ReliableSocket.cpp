#include "ReliableSocket.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>

using namespace std;

ReliableSocket::ReliableSocket()
    : sockfd(-1), state(CLOSED), clientISN(0), serverISN(0),
      threadRunning(false), synAckReceived(false), ackReceived(false)
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cerr << "Error: Failed to create socket." << endl;
        return;
    }
    
    state = CLOSED;
    clientISN = 0;
    serverISN = 0;

    // Set SO_REUSEADDR to allow quick socket rebinding
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Set socket receive timeout (100ms)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Initialize connection structure
    conn.sendSeq = 1000;
    conn.expectedSeq = 0;
}

ReliableSocket::~ReliableSocket()
{
    // Stop threads
    threadRunning = false;
    
    // Wake up threads if they're waiting
    sendCV.notify_all();
    recvCV.notify_all();
    handshakeCV.notify_all();
    ackCV.notify_all();
    
    // Wait for threads to finish
    if (recvThread.joinable()) {
        recvThread.join();
    }
    if (sendThread.joinable()) {
        sendThread.join();
    }
    
    // Close socket
    if (sockfd >= 0) {
        close(sockfd);
    }
}

void ReliableSocket::updateConnectionState(ConnectionState newState)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        state = newState;
    }
}

void ReliableSocket::queueSend(const TransportHeader& header, const std::string& data, Endpoint& endpoint)
{
    SendRequest req;
    req.header = header;
    req.data = data;
    req.endpoint = endpoint;
    
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex);
        sendQueue.push(req);
    }
    sendCV.notify_one();
}

bool ReliableSocket::waitForAck(uint32_t seqNumber, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(ackPendingMutex);
    return ackCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                          [this, seqNumber] { 
                              return ackPendingMap.find(seqNumber) != ackPendingMap.end(); 
                          });
}

bool ReliableSocket::waitForHandshake(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(handshakeMutex);
    return handshakeCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                                [this] { 
                                    return synAckReceived && ackReceived; 
                                });
}

void ReliableSocket::receiveWorker()
{
    cout << "[ReceiveWorker] Started" << endl;
    
    while (threadRunning) {
        TransportHeader header;
        std::string data;
        Endpoint endpoint;
        
        // Receive packet with timeout
        if (!receivePacket(header, data, endpoint)) {
            // Timeout or error, continue loop
            continue;
        }
        
        cout << "[ReceiveWorker] Received packet - flags=" << (int)header.flags 
             << ", seq=" << header.seqNumber 
             << ", ack=" << header.ackNumber << endl;
        
        // Handle handshake packets
        if (header.flags & SYN) {
            if (header.flags & ACK) {
                // SYN-ACK (client receiving from server)
                cout << "[ReceiveWorker] Received SYN-ACK" << endl;
                {
                    std::lock_guard<std::mutex> lock(handshakeMutex);
                    synAckBuffer = header;
                    synAckReceived = true;
                    handshakeEndpoint = endpoint;
                }
                handshakeCV.notify_one();
            } else {
                // SYN (server receiving from client)
                cout << "[ReceiveWorker] Received SYN" << endl;
                {
                    std::lock_guard<std::mutex> lock(handshakeMutex);
                    synAckBuffer = header;  // Store received SYN
                    synAckReceived = true;
                    handshakeEndpoint = endpoint;
                }
                handshakeCV.notify_one();
            }
        } 
        else if (header.flags & ACK && !(header.flags & DATA)) {
            // Pure ACK packet
            cout << "[ReceiveWorker] Received ACK for seq=" << header.ackNumber << endl;
            {
                std::lock_guard<std::mutex> lock(ackPendingMutex);
                ackPendingMap[header.ackNumber] = header;
            }
            ackCV.notify_one();
        } 
        else if (header.flags & DATA) {
            // Data packet
            cout << "[ReceiveWorker] Received DATA - seq=" << header.seqNumber 
                 << ", size=" << data.size() << endl;
            
            {
                std::lock_guard<std::mutex> lock(recvBufferMutex);
                recvBuffer[header.seqNumber] = data;
            }
            recvCV.notify_one();
            
            // Queue ACK to be sent
            TransportHeader ack{};
            ack.flags = ACK;
            ack.ackNumber = header.seqNumber + data.size();
            
            queueSend(ack, "", endpoint);
        }
    }
    
    cout << "[ReceiveWorker] Stopped" << endl;
}

void ReliableSocket::sendWorker()
{
    cout << "[SendWorker] Started" << endl;
    
    while (threadRunning) {
        SendRequest req;
        
        // Wait for data to send
        {
            std::unique_lock<std::mutex> lock(sendQueueMutex);
            if (sendQueue.empty()) {
                sendCV.wait(lock, [this] { return !sendQueue.empty() || !threadRunning; });
            }
            
            if (!threadRunning || sendQueue.empty()) {
                continue;
            }
            
            req = sendQueue.front();
            sendQueue.pop();
        }
        
        cout << "[SendWorker] Sending packet - flags=" << (int)req.header.flags 
             << ", seq=" << req.header.seqNumber << endl;
        
        // Send packet
        if (!sendPacket(req.header, req.data, req.endpoint)) {
            cerr << "[SendWorker] Failed to send packet" << endl;
            continue;
        }
        
        // For handshake SYN packets, wait for appropriate response
        if ((req.header.flags & SYN) && !(req.header.flags & ACK)) {
            cout << "[SendWorker] SYN sent, waiting for response..." << endl;
            
            std::unique_lock<std::mutex> lock(handshakeMutex);
            if (handshakeCV.wait_for(lock, std::chrono::milliseconds(5000), 
                                     [this] { return synAckReceived; })) {
                cout << "[SendWorker] Handshake response received" << endl;
            } else {
                cerr << "[SendWorker] Timeout waiting for handshake response" << endl;
            }
        } 
        else if ((req.header.flags & SYN) && (req.header.flags & ACK)) {
            // SYN-ACK sent - no waiting needed, receiver will respond with ACK
            cout << "[SendWorker] SYN-ACK sent" << endl;
        }
        else if (req.header.flags == ACK) {
            // Pure ACK - no need to wait for response
            cout << "[SendWorker] ACK sent" << endl;
        } 
        else if (req.header.flags & DATA) {
            // Data packet - wait for ACK
            cout << "[SendWorker] DATA sent, waiting for ACK..." << endl;
            
            uint32_t expectedAck = req.header.seqNumber + req.data.size();
            std::unique_lock<std::mutex> lock(ackPendingMutex);
            
            if (ackCV.wait_for(lock, std::chrono::milliseconds(5000), 
                              [this, expectedAck] { 
                                  return ackPendingMap.find(expectedAck) != ackPendingMap.end(); 
                              })) {
                cout << "[SendWorker] ACK received for seq=" << expectedAck << endl;
                ackPendingMap.erase(expectedAck);
            } else {
                cerr << "[SendWorker] Timeout waiting for ACK" << endl;
            }
        }
    }
    
    cout << "[SendWorker] Stopped" << endl;
}

bool ReliableSocket::bindSocket(int port)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Error: Failed to bind socket to port " << port << endl;
        return false;
    }
    
    // Note: Worker threads will be started by connect() or accept()
    cout << "Socket bound to port " << port << endl;
    return true;
}

bool ReliableSocket::sendPacket(const TransportHeader& header,
                                const std::string& data,
                                Endpoint& endpoint)
{
    char buffer[1500];
    memcpy(buffer, &header, sizeof(header));
    
    int totalLen = sizeof(header);
    if (!data.empty()) {
        memcpy(buffer + sizeof(header), data.c_str(), data.size());
        totalLen += data.size();
    }

    int sent = sendto(sockfd, buffer, totalLen, 0,
                      (sockaddr*)&endpoint.addr,
                      sizeof(endpoint.addr));
    return sent > 0;
}

bool ReliableSocket::receivePacket(TransportHeader& header,
                                   std::string& data,
                                   Endpoint& endpoint)
{
    char buffer[1500];
    socklen_t len = sizeof(endpoint.addr);

    int bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                         (sockaddr*)&endpoint.addr, &len);

    // Handle timeout (bytes == -1 with EAGAIN/EWOULDBLOCK)
    if (bytes < 0) {
        return false;
    }

    // Accept packets that are at least as large as the header
    if (bytes < (int)sizeof(TransportHeader)) {
        cerr << "Error: Received packet is too small. Bytes: " << bytes << " Expected: " << sizeof(TransportHeader) << endl;
        return false;
    }

    // Copy the header
    memcpy(&header, buffer, sizeof(header));
    
    // Copy the data if present
    if (bytes > (int)sizeof(TransportHeader)) {
        data.assign(buffer + sizeof(header), bytes - sizeof(header));
    } else {
        data.clear();
    }

    return true;
}

bool ReliableSocket::connect(Endpoint& server)
{
    cout << "Client: Starting connection..." << endl;
    
    // Start worker threads only once
    if (!threadRunning) {
        threadRunning = true;
        recvThread = std::thread(&ReliableSocket::receiveWorker, this);
        sendThread = std::thread(&ReliableSocket::sendWorker, this);
        cout << "Worker threads started" << endl;
    }
    
    // Step 1: Queue SYN packet
    TransportHeader syn{};
    syn.flags = SYN;
    syn.seqNumber = 1000;
    clientISN = syn.seqNumber;

    cout << "Client: Queueing SYN (seqNumber=" << syn.seqNumber << ")" << endl;
    queueSend(syn, "", server);
    
    // Step 2: Wait for SYN-ACK
    cout << "Client: Waiting for SYN-ACK..." << endl;
    {
        std::unique_lock<std::mutex> lock(handshakeMutex);
        if (!handshakeCV.wait_for(lock, std::chrono::milliseconds(5000), 
                                  [this] { return synAckReceived; })) {
            cerr << "Error: Timeout waiting for SYN-ACK" << endl;
            return false;
        }
        
        synAckReceived = false;  // Reset for next handshake phase
        
        if (!(synAckBuffer.flags & SYN) || !(synAckBuffer.flags & ACK)) {
            cerr << "Error: Received packet is not SYN-ACK. Flags=" << (int)synAckBuffer.flags << endl;
            return false;
        }

        if (synAckBuffer.ackNumber != clientISN + 1) {
            cerr << "Error: Invalid ACK number. Expected " << (clientISN + 1) 
                 << ", got " << synAckBuffer.ackNumber << endl;
            return false;
        }

        serverISN = synAckBuffer.seqNumber;
        cout << "Client: Received SYN-ACK (seqNumber=" << serverISN 
             << ", ackNumber=" << synAckBuffer.ackNumber << ")" << endl;
    }

    // Step 3: Queue ACK
    TransportHeader ack{};
    ack.flags = ACK;
    ack.seqNumber = clientISN + 1;
    ack.ackNumber = serverISN + 1;

    cout << "Client: Queueing ACK (seqNumber=" << ack.seqNumber 
         << ", ackNumber=" << ack.ackNumber << ")" << endl;
    queueSend(ack, "", server);
    
    // Wait a bit for ACK to be sent
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    updateConnectionState(ESTABLISHED);
    cout << "Client: Connection established!" << endl;
    return true;
}

bool ReliableSocket::accept(Endpoint& client)
{
    cout << "Server: Starting to accept connection..." << endl;
    
    // Start worker threads only once
    if (!threadRunning) {
        threadRunning = true;
        recvThread = std::thread(&ReliableSocket::receiveWorker, this);
        sendThread = std::thread(&ReliableSocket::sendWorker, this);
        cout << "Worker threads started" << endl;
    }
    
    // Step 1: Wait for SYN
    cout << "Server: Waiting for SYN..." << endl;
    {
        std::unique_lock<std::mutex> lock(handshakeMutex);
        if (!handshakeCV.wait_for(lock, std::chrono::milliseconds(10000), 
                                  [this] { return synAckReceived; })) {
            cerr << "Error: Timeout waiting for SYN" << endl;
            return false;
        }
        
        synAckReceived = false;  // Reset for next handshake phase
        
        if (!(synAckBuffer.flags & SYN)) {
            cerr << "Error: Received packet is not SYN. Flags=" << (int)synAckBuffer.flags << endl;
            return false;
        }

        clientISN = synAckBuffer.seqNumber;
        client = handshakeEndpoint;
        cout << "Server: Received SYN (seqNumber=" << clientISN << ")" << endl;
    }

    // Step 2: Queue SYN-ACK
    TransportHeader synAck{};
    synAck.flags = SYN | ACK;
    synAck.seqNumber = 5000;
    synAck.ackNumber = clientISN + 1;
    serverISN = synAck.seqNumber;

    cout << "Server: Queueing SYN-ACK (seqNumber=" << synAck.seqNumber 
         << ", ackNumber=" << synAck.ackNumber << ")" << endl;
    queueSend(synAck, "", client);

    // Step 3: Wait for ACK
    cout << "Server: Waiting for ACK..." << endl;
    {
        std::unique_lock<std::mutex> lock(ackPendingMutex);
        uint32_t expectedAck = serverISN + 1;
        if (!ackCV.wait_for(lock, std::chrono::milliseconds(10000),  // Increased to 10 seconds
                           [this, expectedAck] { 
                               return ackPendingMap.find(expectedAck) != ackPendingMap.end(); 
                           })) {
            cerr << "Error: Timeout waiting for ACK" << endl;
            cerr << "Expected ACK=" << expectedAck << endl;
            cerr << "ackPendingMap contents: ";
            for (auto& p : ackPendingMap) {
                cerr << p.first << " ";
            }
            cerr << endl;
            return false;
        }
        
        ackPendingMap.erase(expectedAck);
        cout << "Server: Received ACK (seqNumber=?, ackNumber=" << expectedAck << ")" << endl;
    }

    updateConnectionState(ESTABLISHED);
    cout << "Server: Connection established!" << endl;
    return true;
}

bool ReliableSocket::reliableSend(const std::string& data, Endpoint& endpoint)
{
    if (state != ESTABLISHED) {
        cerr << "Error: Connection not established" << endl;
        return false;
    }
    
    TransportHeader header{};
    header.flags = DATA;
    header.seqNumber = conn.sendSeq;

    cout << "Application: Queueing data to send (" << data.size() << " bytes)" << endl;
    queueSend(header, data, endpoint);
    
    // Wait for ACK
    uint32_t expectedAck = conn.sendSeq + data.size();
    cout << "Application: Waiting for ACK for seq=" << expectedAck << endl;
    
    bool ackReceived = waitForAck(expectedAck, 5000);
    
    if (ackReceived) {
        cout << "Application: ACK received! Message sent successfully." << endl;
        conn.sendSeq += data.size();
        return true;
    } else {
        cerr << "Application: Timeout waiting for ACK" << endl;
        return false;
    }
}

std::string ReliableSocket::reliableRecv(Endpoint& endpoint)
{
    if (state != ESTABLISHED) {
        cerr << "Error: Connection not established" << endl;
        return "";
    }
    
    cout << "Application: Waiting for incoming data..." << endl;
    
    // Wait for data to arrive in buffer
    uint32_t dataSeq = 0;
    std::string receivedData;
    
    {
        std::unique_lock<std::mutex> lock(recvBufferMutex);
        
        // Wait until data arrives
        bool hasData = recvCV.wait_for(lock, std::chrono::milliseconds(10000), 
                                       [this] { return !recvBuffer.empty(); });
        
        if (!hasData) {
            cerr << "Error: Timeout waiting for data" << endl;
            return "";
        }
        
        // Get the first available packet
        auto it = recvBuffer.begin();
        dataSeq = it->first;
        receivedData = it->second;
        recvBuffer.erase(it);
    }
    
    cout << "Application: Received data (" << receivedData.size() 
         << " bytes) with seq=" << dataSeq << endl;
    
    return receivedData;
}


