/**
 * tcp_client.h - TCP client for sending files
 *
 * The TcpClient class handles the sending side of a file transfer.
 * It connects to a remote TcpServer, optionally encrypts the file data,
 * sends it in chunks, and waits for a confirmation (ACK or ERR).
 *
 * Usage:
 *   TcpClient client;
 *   bool ok = client.send_file("192.168.1.50", 9090, "./photo.jpg");
 *   // With encryption:
 *   bool ok = client.send_file("192.168.1.50", 9090, "./secret.txt", "mypass");
 */
#pragma once

#include "platform.h"
#include "protocol.h"
#include <string>
#include <functional>

namespace uair {

class TcpClient {
public:
    /** Callback type for progress updates during file transfer */
    using ProgressCallback = std::function<void(uint64_t sent, uint64_t total)>;

    /**
     * send_file() - Send a file to a remote TcpServer
     *
     * This function:
     *   1. Reads the file from disk into memory
     *   2. If a passphrase is provided, encrypts the file data with AES-256-GCM
     *   3. Connects to the remote server via TCP
     *   4. Sends the file header (filename, size, encrypted flag)
     *   5. Sends the payload in 4KB chunks, calling the progress callback
     *   6. Waits for an ACK or ERR response from the server
     *   7. Closes the connection and returns success/failure
     *
     * @param ip          The target IP address (e.g., "192.168.1.50")
     * @param port        The target TCP port (e.g., 9090)
     * @param filepath    Path to the local file to send
     * @param passphrase  Optional encryption passphrase (empty = no encryption)
     * @param progress    Optional callback invoked after each chunk is sent
     * @return            true if the server acknowledged success, false otherwise
     */
    bool send_file(const std::string& ip, uint16_t port,
                   const std::string& filepath,
                   const std::string& passphrase = "",
                   ProgressCallback progress = nullptr);

private:
    /**
     * send_all() - Send all bytes through a socket, retrying as needed
     *
     * The send() system call doesn't guarantee it will send all the
     * bytes you request — on a slow network it might only send a
     * portion. This function loops until every byte has been sent.
     *
     * @param sock  The connected socket to send on
     * @param data  Pointer to the data buffer
     * @param len   Number of bytes to send
     * @return      true if all bytes were sent, false on error
     */
    bool send_all(socket_t sock, const char* data, size_t len);
};

} // namespace uair