#include "../transport/ReliableSocket.h"
#include <iostream>

int main() {

    ReliableSocket server;
    server.bind(8080);

    Endpoint client;

    while(true) {

        std::string msg = server.recv(client);

        if(msg != "") {
            std::cout << "Client: " << msg << std::endl;
            server.send(msg, client);
        }
    }
}
