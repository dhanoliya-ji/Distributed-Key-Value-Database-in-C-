#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>
#include "network.hpp"

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = 8000;

    // Parse options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "      Distributed Key-Value Database Client       " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Connecting to server at " << ip << ":" << port << "..." << std::endl;

    if (!Network::init()) {
        return 1;
    }

    SOCKET clientSock = Network::connectToNode(ip, port);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Connection failed: " << Network::getLastErrorStr() << std::endl;
        Network::cleanup();
        return 1;
    }

    std::cout << "Connected successfully." << std::endl;
    std::cout << "Type SQL queries or raw commands. Type 'exit' to quit." << std::endl;
    std::cout << "Example SQL: INSERT INTO kv VALUES ('user:1', 'Alice');" << std::endl;
    std::cout << "             SELECT * FROM kv;" << std::endl;
    std::cout << "Example Raw: PUT mykey myvalue" << std::endl;
    std::cout << "             GET mykey" << std::endl;
    std::cout << "==================================================" << std::endl;

    std::string input;
    while (true) {
        std::cout << "kv-db> ";
        std::getline(std::cin, input);
        
        // Trim input
        size_t first = input.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }
        std::string trimmed = input.substr(first);
        size_t last = trimmed.find_last_not_of(" \t\r\n");
        trimmed = trimmed.substr(0, last + 1);

        std::string upperTrimmed = trimmed;
        std::transform(upperTrimmed.begin(), upperTrimmed.end(), upperTrimmed.begin(), ::toupper);

        if (upperTrimmed == "EXIT" || upperTrimmed == "QUIT") {
            break;
        }

        if (trimmed.empty()) {
            continue;
        }

        if (!Network::sendString(clientSock, trimmed)) {
            std::cerr << "Error: Failed to send data to server." << std::endl;
            break;
        }

        // Read response until EOT character (\x04)
        std::string line;
        while (true) {
            if (!Network::recvString(clientSock, line)) {
                std::cerr << "Error: Lost connection to server." << std::endl;
                break;
            }
            if (line == "\x04") {
                break;
            }
            std::cout << line << std::endl;
        }
    }

    Network::closeSocket(clientSock);
    Network::cleanup();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
