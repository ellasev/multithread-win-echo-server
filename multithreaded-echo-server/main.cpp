#include <iostream>
#include <thread>
#include <list>
#include <Ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "12345"

int handle_connection(SOCKET socket) {
    int ret = 0;
    int res;
    int sendRes;

    char recvBuf[DEFAULT_BUFLEN];
    int recvBufLen = DEFAULT_BUFLEN;

    std::cout << "[CT] starting" << std::endl;
    // Receive until the peer shuts down the connection
    do {
        std::cout << "[CT] receiving data" << std::endl;
        res = recv(socket, recvBuf, recvBufLen, 0);
        if (res > 0) {
            std::cout << "[CT] Bytes received: " << res << std::endl;

            // Echo the buffer back to the sender
            std::cout << "[CT] sending data back" << std::endl;
            sendRes = send(socket, recvBuf, res, 0);
            if (sendRes == SOCKET_ERROR) {
                std::cout << "[CT] send failed with error: " << WSAGetLastError() << std::endl;
                ret = 1;
            }
            else if (sendRes != res) {
                std::cout << "[CT] sent different ammount than bytes received. " << std::endl;
                ret = 1;
            }
            std::cout << "[CT] Bytes sent: " << sendRes << std::endl;;
        }
        else if (res == 0)
            std::cout << "[CT] Connection closing..." << std::endl;
        else {
            std::cout << "[CT] recv failed with error: " << WSAGetLastError() << std::endl;
            ret = 1;
        }

    } while (res > 0 && ret == 0);

    std::cout << "[CT] closing connection" << std::endl;
    // shutdown the connection since we're done
    res = shutdown(socket, SD_SEND);
    if (res == SOCKET_ERROR) {
        std::cout << "[CT] shutdown failed with error: " << WSAGetLastError() << std::endl;
        ret = 1;
    }

    if (closesocket(socket) != 0) {
        std::cout << "[CT] Closing client socket failed with error: " << WSAGetLastError() << std::endl;
        ret = 1;
    }
    return ret;
}

int initialize_socket(SOCKET& listenSocket) {
    int res;

    WSADATA wsaData;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cout << "WSAStartup failed with error: " << res << std::endl;
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    res = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (res != 0) {
        std::cout << "getaddrinfo failed with error: " << res << std::endl;
        return 1;
    }

    // Create a SOCKET for connecting to server
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        return 1;
    }

    // Setup the TCP listening socket
    res = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (res == SOCKET_ERROR) {
        std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
        return 2;
    }

    std::cout << "Listening" << std::endl;
    // Start listening on socket.
    res = listen(listenSocket, SOMAXCONN);
    if (res == SOCKET_ERROR) {
        std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
        return 2;
    }

    return 0;
}

int __cdecl main(void)
{
    int ret = 1;
    int res;

    SOCKET listenSocket = INVALID_SOCKET;
    SOCKET currentSocket = INVALID_SOCKET;

    std::list<std::thread> clientThreads;

    res = initialize_socket(listenSocket);
    if (res == 1) {
        goto cleanup;
    }
    else if (res == 2) {
        goto server_cleanup;
    }

    while (TRUE) {
        std::cout << "Waiting for connection" << std::endl;
        // Accept a client socket
        currentSocket = accept(listenSocket, NULL, NULL);
        if (currentSocket == INVALID_SOCKET) {
            std::cout << "accept failed with error: %d\n" << std::endl;
            goto server_cleanup;
        }
        std::cout << "Starting client logic" << std::endl;
        clientThreads.push_back(std::thread(handle_connection, currentSocket));
    }

server_cleanup:
    for (auto& clientThread : clientThreads) {
        clientThread.join();
    }

    if (closesocket(listenSocket) != 0) {
        std::cout << "Closing listening socket failed with error: " << WSAGetLastError() << std::endl;
    }
cleanup:
    if (WSACleanup() != 0) {
        std::cout << "WSA cleanup failed with error: " << WSAGetLastError() << std::endl;
    }

    return 1;
}