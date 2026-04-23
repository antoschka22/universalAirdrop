#pragma once

// Cross-platform networking and OS abstraction layer
// Handles: Winsock vs POSIX sockets, signal handling, directory creation, OS detection

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
    #define SOCKLEN_T int

    inline int init_networking() {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
    }

    inline void cleanup_networking() { WSACleanup(); }

    // Windows doesn't have POSIX signals — use SetConsoleCtrlHandler instead
    #include <windows.h>
    #include <csignal>
    // SIGINT still works on Windows but we provide an abstraction for graceful shutdown
    inline void install_signal_handler(void(*handler)(int)) {
        signal(SIGINT, handler);
    }

    inline int make_directory(const char* path) { return _mkdir(path); }
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <cstring>
    #include <csignal>
    #include <sys/stat.h>

    using socket_t = int;
    #define INVALID_SOCK (-1)
    #define SOCK_CLOSE close
    #define SOCKLEN_T socklen_t

    inline int init_networking() { return 0; }
    inline void cleanup_networking() {}

    inline void install_signal_handler(void(*handler)(int)) {
        struct sigaction sa{};
        sa.sa_handler = handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    inline int make_directory(const char* path) { return mkdir(path, 0755); }
#endif

// OS name for discovery broadcasts
inline const char* get_os_name() {
#ifdef _WIN32
    return "Windows";
#elif __APPLE__
    return "macOS";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
#else
    return "Unknown";
#endif
}