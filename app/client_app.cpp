#include "../transport/ReliableSocket.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

int main()
{
    ReliableSocket sock;
    Endpoint server("127.0.0.1", 9000);

    cout << "=== Reliable UDP Client ===" << endl;
    cout << "Connecting to server at 127.0.0.1:9000..." << endl;
    
    if (!sock.connect(server)) {
        cerr << "Failed to connect to server." << endl;
        return 1;
    }

    cout << "\n✓ Connected to server!" << endl;
    cout << "\n--- Test Messages ---" << endl;

    // Test 1: Simple message
    string msg1 = "Hello from Client!";
    cout << "\n[Test 1] Sending: " << msg1 << endl;
    if (!sock.reliableSend(msg1, server)) {
        cerr << "Failed to send message 1." << endl;
    }

    // Test 2: Message with numbers
    string msg2 = "Test Message #2 - Sequence Number Test";
    cout << "[Test 2] Sending: " << msg2 << endl;
    if (!sock.reliableSend(msg2, server)) {
        cerr << "Failed to send message 2." << endl;
    }

    // Test 3: Longer message
    string msg3 = "This is a longer test message to verify that the reliable UDP protocol can handle messages of varying lengths without any data loss or corruption.";
    cout << "[Test 3] Sending: " << msg3 << endl;
    if (!sock.reliableSend(msg3, server)) {
        cerr << "Failed to send message 3." << endl;
    }

    // Test 4: Multiple rapid messages
    cout << "\n[Test 4] Sending multiple rapid messages:" << endl;
    for (int i = 1; i <= 5; i++) {
        stringstream ss;
        ss << "Rapid Test Message #" << i;
        string msg = ss.str();
        cout << "  - " << msg << endl;
        if (!sock.reliableSend(msg, server)) {
            cerr << "Failed to send rapid message " << i << "." << endl;
        }
    }

    // Test 5: Interactive mode
    cout << "\n--- Interactive Mode ---" << endl;
    cout << "Type messages to send (type 'quit' to exit):" << endl;

    string message;
    int count = 0;
    while (getline(cin, message)) {
        if (message == "quit") {
            cout << "Exiting..." << endl;
            break;
        }

        if (!message.empty()) {
            count++;
            cout << "[Message " << count << "] Sending: " << message;
            if (!sock.reliableSend(message, server)) {
                cerr << " - FAILED" << endl;
            } else {
                cout << " - OK" << endl;
            }
        }
    }

    cout << "\n✓ Client test completed!" << endl;
    return 0;
}

