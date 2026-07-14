#include "network.hpp"
#include <iostream>
#include <vector>

bool Network::init() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "WSAStartup failed: " << res << std::endl;
        return false;
    }
    return true;
}

void Network::cleanup() {
    WSACleanup();
}

SOCKET Network::listenOnPort(int port) {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << getLastErrorStr() << std::endl;
        return INVALID_SOCKET;
    }

    // Set socket options (SO_REUSEADDR)
    char optval = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(port);

    if (bind(listenSock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << getLastErrorStr() << std::endl;
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << getLastErrorStr() << std::endl;
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    return listenSock;
}

SOCKET Network::connectToNode(const std::string& ip, int port) {
    SOCKET connectSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << getLastErrorStr() << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in clientService{};
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &clientService.sin_addr);

    if (connect(connectSock, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        // Don't print error here, as we might try reconnection in loop
        closesocket(connectSock);
        return INVALID_SOCKET;
    }

    return connectSock;
}

bool Network::sendString(SOCKET sock, const std::string& str) {
    std::string packet = str;
    if (packet.empty() || packet.back() != '\n') {
        packet += '\n';
    }

    size_t totalSent = 0;
    size_t len = packet.length();
    const char* buf = packet.c_str();

    while (totalSent < len) {
        int sent = send(sock, buf + totalSent, static_cast<int>(len - totalSent), 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool Network::recvString(SOCKET sock, std::string& str) {
    str.clear();
    char c;
    while (true) {
        int res = recv(sock, &c, 1, 0);
        if (res > 0) {
            if (c == '\n') {
                break;
            }
            str += c;
        } else {
            return false; // Connection closed or error
        }
    }
    return true;
}

void Network::closeSocket(SOCKET sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

std::string Network::getLastErrorStr() {
    int err = WSAGetLastError();
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr
    );
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    
    // Trim trailing whitespace
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
        message.pop_back();
    }
    return message + " (Code: " + std::to_string(err) + ")";
}
