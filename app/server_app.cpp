#include "../transport/ReliableSocket.h"
#include <iostream>
#include <string>
#include <ctime>

using namespace std;

int main()
{
    ReliableSocket sock;

    cout << "=== Reliable UDP Server ===" << endl;
    cout << "Binding to port 9000..." << endl;

    if (!sock.bindSocket(9000)) {
        cerr << "Failed to bind socket." << endl;
        return 1;
    }

    cout << "✓ Server is running on port 9000" << endl;
    cout << "Waiting for client connection..." << endl;

    Endpoint client;
    if (!sock.accept(client)) {
        cerr << "Failed to accept client connection." << endl;
        return 1;
    }

    cout << "\n✓ Client connected!" << endl;
    cout << "\n--- Receiving Messages ---" << endl;

    int messageCount = 0;
    long long totalBytesReceived = 0;
    time_t startTime = time(nullptr);

    while (true) {
        string msg = sock.reliableRecv(client);
        if (msg.empty()) {
            cout << "\nConnection closed by client." << endl;
            break;
        }

        messageCount++;
        totalBytesReceived += msg.size();
        time_t currentTime = time(nullptr);
        
        // Display message info
        cout << "\n[Message " << messageCount << "] Received (" << (currentTime - startTime) << "s): ";
        cout << msg.size() << " bytes";
        
        // Show first 50 chars if message is large
        if (msg.size() > 50) {
            cout << " [" << msg.substr(0, 50) << "...]" << endl;
        } else {
            cout << " [" << msg << "]" << endl;
        }
    }

    cout << "\n--- Test Summary ---" << endl;
    cout << "Total messages received: " << messageCount << endl;
    cout << "Total bytes received: " << totalBytesReceived << " bytes" << endl;
    cout << "Average message size: " << (messageCount > 0 ? totalBytesReceived / messageCount : 0) << " bytes" << endl;
    cout << "✓ Server test completed!" << endl;

    return 0;
}

