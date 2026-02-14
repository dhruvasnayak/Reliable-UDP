#include "../transport/ReliableSocket.h"
#include <iostream>
#include <arpa/inet.h>

int main() {

    ReliableSocket client;

    Endpoint server{};
    server.addr.sin_family = AF_INET;
    server.addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server.addr.sin_addr);

    while(true) {

        std::string msg;
        std::getline(std::cin, msg);

        client.send(msg, server);

        Endpoint reply;
        std::string res = client.recv(reply);

        std::cout << "Server: " << res << std::endl;
    }
}
