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
    time_t startTime = time(nullptr);

    while (true) {
        string msg = sock.reliableRecv(client);
        if (msg.empty()) {
            cout << "\nConnection closed by client." << endl;
            break;
        }

        messageCount++;
        time_t currentTime = time(nullptr);
        cout << "\n[Message " << messageCount << "] Received (" << (currentTime - startTime) << "s): " << msg << endl;
    }

    cout << "\n--- Test Summary ---" << endl;
    cout << "Total messages received: " << messageCount << endl;
    cout << "✓ Server test completed!" << endl;

    return 0;
}

