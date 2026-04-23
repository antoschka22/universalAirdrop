/**
 * platform.h - Cross-platform networking and OS abstraction layer
 *
 * This file is the foundation of the project's cross-platform support.
 * Windows and Unix (macOS/Linux) have very different networking APIs:
 *   - Windows uses "Winsock" (Windows Sockets) which requires WSAStartup/WSACleanup
 *   - Unix uses "POSIX sockets" which are just file descriptors
 *
 * Instead of sprinkling #ifdef _WIN32 throughout the codebase, ALL platform
 * differences are centralized here. Every other file includes this header
 * and uses the abstracted types/macros/functions, keeping the rest of the
 * code portable and clean.
 *
 * What this file provides:
 *   1. Socket type abstraction (socket_t, INVALID_SOCK, SOCK_CLOSE)
 *   2. Networking init/cleanup (WSAStartup on Windows, no-op on Unix)
 *   3. Signal handling (sigaction on Unix, signal() on Windows)
 *   4. Directory creation (_mkdir on Windows, mkdir with permissions on Unix)
 *   5. OS name detection (for discovery broadcasts)
 */
#pragma once

#ifdef _WIN32
    /* --- Windows section ---
     * Winsock requires explicit initialization via WSAStartup() before
     * any socket functions can be used, and WSACleanup() when done.
     * The WIN32_LEAN_AND_MEAN macro excludes rarely-used Windows headers
     * to speed up compilation. */
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>   /* inet_pton, inet_ntop, sockaddr_in */
    #include <iphlpapi.h>   /* Network adapter info (for future use) */
    #pragma comment(lib, "ws2_32.lib")  /* Tell the linker to link ws2_32.lib */

    /* SOCKET is an unsigned int on Windows (a kernel handle), NOT a file
     * descriptor. INVALID_SOCKET is a special value (usually ~0) that
     * indicates failure. closesocket() is the Windows equivalent of close(). */
    using socket_t = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCK_CLOSE closesocket

    /* On Windows, the address length parameter for accept()/recvfrom() is
     * typed as int, not socklen_t. */
    #define SOCKLEN_T int

    /**
     * init_networking() - Initialize Windows Sockets
     * On Windows, this calls WSAStartup() which loads the Winsock DLL.
     * Returns 0 on success, -1 on failure.
     */
    inline int init_networking() {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
    }

    /**
     * cleanup_networking() - Release Windows Sockets resources
     * Must be called before program exit on Windows.
     */
    inline void cleanup_networking() { WSACleanup(); }

    /* Windows signal handling: signal() works for SIGINT on Windows.
     * For more sophisticated control, SetConsoleCtrlHandler can be used
     * in the future. */
    #include <windows.h>
    #include <csignal>
    inline void install_signal_handler(void(*handler)(int)) {
        signal(SIGINT, handler);
    }

    /* _mkdir() is the Windows equivalent of mkdir(). It takes a single
     * path argument (no permissions). */
    inline int make_directory(const char* path) { return _mkdir(path); }
#else
    /* --- Unix (macOS/Linux) section ---
     * POSIX sockets don't require initialization. They are regular file
     * descriptors, so close() is used to close them (same function used
     * for closing files). */
    #include <sys/socket.h>    /* socket(), bind(), listen(), accept(), etc. */
    #include <arpa/inet.h>     /* inet_pton(), inet_ntop(), htons() */
    #include <netinet/in.h>    /* sockaddr_in, INADDR_ANY */
    #include <netdb.h>         /* getaddrinfo() (for future DNS support) */
    #include <unistd.h>        /* close(), used as SOCK_CLOSE */
    #include <cstring>
    #include <csignal>         /* sigaction() for clean signal handling */
    #include <sys/stat.h>      /* mkdir() with permissions */

    /* On Unix, a socket is just an int (a file descriptor). -1 indicates
     * failure from socket(), accept(), etc. close() is used to close it. */
    using socket_t = int;
    #define INVALID_SOCK (-1)
    #define SOCK_CLOSE close

    /* socklen_t is the proper POSIX type for address lengths. */
    #define SOCKLEN_T socklen_t

    /** No initialization needed on Unix - sockets work out of the box. */
    inline int init_networking() { return 0; }

    /** No cleanup needed on Unix. */
    inline void cleanup_networking() {}

    /**
     * install_signal_handler() - Set up graceful shutdown on SIGINT/SIGTERM
     * Uses sigaction() instead of signal() because signal() has inconsistent
     * behavior across Unix implementations (some reset the handler after
     * one invocation). sigaction() is the modern POSIX standard and
     * behaves consistently. We handle both SIGINT (Ctrl+C) and SIGTERM
     * (kill command) so the program shuts down cleanly in both cases.
     */
    inline void install_signal_handler(void(*handler)(int)) {
        struct sigaction sa{};
        sa.sa_handler = handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    /** mkdir() on Unix takes a permissions mode. 0755 means:
     *  - Owner: read, write, execute (7 = 4+2+1)
     *  - Group: read, execute (5 = 4+1)
     *  - Others: read, execute (5 = 4+1) */
    inline int make_directory(const char* path) { return mkdir(path, 0755); }
#endif

/**
 * get_os_name() - Return a human-readable OS name string
 * Uses compiler-defined macros to detect the platform at compile time.
 * This string is included in UDP discovery broadcasts so users can
 * see what OS a remote device is running (e.g., "Windows", "macOS").
 */
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