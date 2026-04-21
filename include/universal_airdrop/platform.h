#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "ws2_32.lib")

    using socket_t = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCK_CLOSE closesocket

    inline int init_networking() {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    inline void cleanup_networking() {
        WSACleanup();
    }
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <cstring>

    using socket_t = int;
    #define INVALID_SOCK (-1)
    #define SOCK_CLOSE close

    inline int init_networking() { return 0; }
    inline void cleanup_networking() {}
#endif