/**
 * tcp_server.h - TCP server for receiving files
 *
 * The TcpServer class listens on a specified port and accepts incoming
 * file transfer connections. It runs continuously in the background,
 * spawning a new thread for each incoming connection so multiple files
 * can be received simultaneously.
 *
 * Usage flow:
 *   1. Create a TcpServer with a port number
 *   2. Optionally set a receive directory and/or passphrase
 *   3. Call start() — this binds the port and begins listening
 *   4. Received files are saved to the receive directory
 *   5. Call stop() to shut down cleanly
 *
 * The server handles TCP stream framing by using a newline delimiter
 * between the header and file data. It also supports decryption
 * when a passphrase has been set via set_passphrase().
 */
#pragma once

#include "platform.h"
#include "protocol.h"
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace uair {

class TcpServer {
public:
    /** Callback type for progress updates (not currently used by server) */
    using ProgressCallback = std::function<void(uint64_t sent, uint64_t total)>;

    /**
     * Construct a TCP server that listens on the given port.
     * @param port  The TCP port to listen on (default: 9090)
     */
    explicit TcpServer(uint16_t port = DEFAULT_TCP_PORT);

    /** Stops the server and closes the listening socket. */
    ~TcpServer();

    /**
     * Start the server: bind the port, begin listening, and launch
     * the accept loop on a background thread.
     * @return true if the server started successfully, false on error
     */
    bool start();

    /** Stop the server and join the accept thread. */
    void stop();

    /**
     * Set the directory where received files will be saved.
     * @param dir  A filesystem path (will be created if it doesn't exist)
     */
    void set_receive_dir(const std::string& dir);

    /**
     * Set the passphrase for decrypting incoming files.
     * If a passphrase is set, the server will attempt to decrypt
     * any file that arrives with the encrypted flag. If the
     * passphrase is wrong, decryption fails and no file is written.
     * @param passphrase  The decryption key, or empty for no decryption
     */
    void set_passphrase(const std::string& passphrase);

private:
    /**
     * accept_loop() - Background thread that accepts incoming connections
     *
     * Runs in a loop calling accept() on the listening socket. When a
     * client connects, it extracts the client's IP address and spawns
     * a detached thread to handle the connection via handle_client().
     */
    void accept_loop();

    /**
     * handle_client() - Handle a single file transfer from a connected client
     *
     * This is the most complex function in the server. It:
     *   1. Reads data from the socket until it finds the \n header delimiter
     *   2. Parses the header to get filename, size, and encrypted flag
     *   3. Reads the remaining file data (handling any overflow from step 1)
     *   4. If encrypted, decrypts the payload using the set passphrase
     *   5. Writes the (decrypted) data to disk
     *   6. Sends an ACK or ERR response back to the client
     *
     * @param client_sock  The socket for this specific client connection
     * @param client_ip    The IP address of the connected client (for logging)
     */
    void handle_client(socket_t client_sock, const std::string& client_ip);

    uint16_t port_;                  /**< The port this server listens on */
    std::string receive_dir_ = "./received"; /**< Directory for saved files */
    std::string passphrase_;         /**< Optional decryption passphrase */
    socket_t listen_sock_ = INVALID_SOCK; /**< The listening socket */
    std::atomic<bool> running_{false}; /**< Flag to signal the accept loop to stop */
    std::thread accept_thread_;      /**< Thread running accept_loop() */
};

} // namespace uair