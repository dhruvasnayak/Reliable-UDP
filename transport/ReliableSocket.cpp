#include "ReliableSocket.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace std;

ReliableSocket::ReliableSocket()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cerr << "Error: Failed to create socket." << endl;
    }
    state = CLOSED;
    clientISN = 0;
    serverISN = 0;

    // Set SO_REUSEADDR to allow quick socket rebinding
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
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

    // Accept packets that are at least as large as the header
    if (bytes < (int)sizeof(TransportHeader)) {
        if (bytes < 0) {
            perror("recvfrom");
        }
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
    // Step 1: Send SYN
    TransportHeader syn{};
    syn.flags = SYN;
    syn.seqNumber = 1000;
    clientISN = syn.seqNumber;

    cout << "Client: Sending SYN (seqNumber=" << syn.seqNumber << ")" << endl;
    if (!sendPacket(syn, "", server)) {
        cerr << "Error: Failed to send SYN packet." << endl;
        return false;
    }

    // Step 2: Receive SYN-ACK
    TransportHeader synAck;
    std::string data;
    cout << "Client: Waiting for SYN-ACK..." << endl;
    if (!receivePacket(synAck, data, server)) {
        cerr << "Error: Failed to receive SYN-ACK packet." << endl;
        return false;
    }

    if (!(synAck.flags & SYN) || !(synAck.flags & ACK)) {
        cerr << "Error: Received packet is not SYN-ACK. Flags=" << (int)synAck.flags << endl;
        return false;
    }

    if (synAck.ackNumber != clientISN + 1) {
        cerr << "Error: Invalid ACK number. Expected " << (clientISN + 1) << ", got " << synAck.ackNumber << endl;
        return false;
    }

    serverISN = synAck.seqNumber;
    cout << "Client: Received SYN-ACK (seqNumber=" << serverISN << ", ackNumber=" << synAck.ackNumber << ")" << endl;

    // Step 3: Send ACK
    TransportHeader ack{};
    ack.flags = ACK;
    ack.seqNumber = clientISN + 1;
    ack.ackNumber = serverISN + 1;

    cout << "Client: Sending ACK (seqNumber=" << ack.seqNumber << ", ackNumber=" << ack.ackNumber << ")" << endl;
    if (!sendPacket(ack, "", server)) {
        cerr << "Error: Failed to send ACK packet." << endl;
        return false;
    }

    state = ESTABLISHED;
    cout << "Client: Connection established!" << endl;
    return true;
}

bool ReliableSocket::accept(Endpoint& client)
{
    // Step 1: Receive SYN
    TransportHeader syn;
    std::string data;
    cout << "Server: Waiting for SYN..." << endl;
    if (!receivePacket(syn, data, client)) {
        cerr << "Error: Failed to receive SYN packet." << endl;
        return false;
    }

    if (!(syn.flags & SYN)) {
        cerr << "Error: Received packet is not SYN. Flags=" << (int)syn.flags << endl;
        return false;
    }

    clientISN = syn.seqNumber;
    cout << "Server: Received SYN (seqNumber=" << clientISN << ")" << endl;

    // Step 2: Send SYN-ACK
    TransportHeader synAck{};
    synAck.flags = SYN | ACK;
    synAck.seqNumber = 5000;
    synAck.ackNumber = clientISN + 1;
    serverISN = synAck.seqNumber;

    cout << "Server: Sending SYN-ACK (seqNumber=" << synAck.seqNumber << ", ackNumber=" << synAck.ackNumber << ")" << endl;
    if (!sendPacket(synAck, "", client)) {
        cerr << "Error: Failed to send SYN-ACK packet." << endl;
        return false;
    }

    // Step 3: Receive ACK
    TransportHeader ack;
    cout << "Server: Waiting for ACK..." << endl;
    if (!receivePacket(ack, data, client)) {
        cerr << "Error: Failed to receive ACK packet." << endl;
        return false;
    }

    if (!(ack.flags & ACK)) {
        cerr << "Error: Received packet is not ACK. Flags=" << (int)ack.flags << endl;
        return false;
    }

    if (ack.ackNumber != serverISN + 1) {
        cerr << "Error: Invalid ACK number. Expected " << (serverISN + 1) << ", got " << ack.ackNumber << endl;
        return false;
    }

    cout << "Server: Received ACK (seqNumber=" << ack.seqNumber << ", ackNumber=" << ack.ackNumber << ")" << endl;

    state = ESTABLISHED;
    cout << "Server: Connection established!" << endl;
    return true;
}

bool ReliableSocket::reliableSend(const std::string& data, Endpoint& endpoint)
{
    TransportHeader header{};
    header.flags = DATA;
    header.seqNumber = conn.sendSeq;

    cout << "Sending: " << data << endl;
    if (!sendPacket(header, data, endpoint)) {
        cerr << "Error: Failed to send data packet." << endl;
        return false;
    }

    // Wait for ACK
    TransportHeader ack;
    std::string dummy;
    if (!receivePacket(ack, dummy, endpoint)) {
        cerr << "Error: Failed to receive ACK for data packet." << endl;
        return false;
    }

    if (!(ack.flags & ACK)) {
        cerr << "Error: Expected ACK but got flags=" << (int)ack.flags << endl;
        return false;
    }

    conn.sendSeq += data.size();
    return true;
}

std::string ReliableSocket::reliableRecv(Endpoint& endpoint)
{
    TransportHeader header;
    std::string data;

    if (!receivePacket(header, data, endpoint)) {
        cerr << "Error: Failed to receive data packet." << endl;
        return "";
    }

    if (!(header.flags & DATA)) {
        cerr << "Error: Expected DATA packet but got flags=" << (int)header.flags << endl;
        return "";
    }

    // Send ACK
    TransportHeader ack{};
    ack.flags = ACK;
    ack.ackNumber = header.seqNumber + data.size();
    if (!sendPacket(ack, "", endpoint)) {
        cerr << "Error: Failed to send ACK for data packet." << endl;
    }

    return data;
}


