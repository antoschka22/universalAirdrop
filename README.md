# Universal Airdrop

A cross-platform local file-sharing application inspired by Apple's AirDrop. Send files between devices on the same Wi-Fi network — Windows, macOS, Linux, or any combination — without a central server.

## How It Works

Universal Airdrop uses two networking protocols working together:

1. **UDP Discovery** — Each device broadcasts its presence on the local network every 2 seconds. The payload contains the device name, OS, and TCP port. Other devices listen for these broadcasts and build a list of nearby peers.

2. **TCP Transfer** — When you send a file, a direct TCP connection is established to the target device. The file header (filename + size) is sent first, followed by the file data in 4KB chunks. The receiver writes the file to disk and sends back an ACK/ERR response.

### Encryption

When a passphrase is provided, files are encrypted with **AES-256-GCM** before transfer:

- A random 16-byte salt and 12-byte IV are generated per transfer
- The encryption key is derived from the passphrase using **PBKDF2-SHA256** (100,000 iterations)
- Wire format: `[16-byte salt][12-byte IV][ciphertext][16-byte GCM auth tag]`
- Wrong passphrases are rejected by GCM authentication — no data is written to disk

## Architecture

```
include/universal_airdrop/
  platform.h          Cross-platform abstraction (Winsock vs POSIX sockets)
  protocol.h          Message types, serialization, parsing
  tcp_server.h        TCP listener for incoming file transfers
  tcp_client.h        TCP sender for outgoing file transfers
  udp_discovery.h     UDP broadcast/listen for device discovery
  file_transfer.h     File I/O utilities
  crypto.h            AES-256-GCM encryption/decryption (OpenSSL EVP)
  gui_helpers.h       Qt bridge: networking workers with Qt signals
  main_window.h       Qt main window class

src/
  main.cpp            CLI entry point (discover / send / receive)
  tcp_server.cpp
  tcp_client.cpp
  udp_discovery.cpp
  file_transfer.cpp
  crypto.cpp

src/gui/
  main_window.cpp     Qt GUI implementation
  main_window.ui      Qt Designer UI file (tabs, progress bar, file picker)
  gui_helpers.cpp     Background thread workers for Qt signals

src/
  gui_main.cpp        Qt application entry point
```

## Technologies

| Component | Technology |
|---|---|
| Language | C++17 |
| Networking (POSIX) | `<sys/socket.h>`, `<arpa/inet.h>`, `<netinet/in.h>` |
| Networking (Windows) | Winsock2 (`<winsock2.h>`, `<ws2tcpip.h>`) |
| Encryption | OpenSSL 3.x — EVP API (AES-256-GCM, PBKDF2-SHA256) |
| File I/O | `<filesystem>` (C++17) |
| Threading | `<thread>`, `<mutex>`, `<atomic>` |
| GUI | Qt 6 (Widgets, Network) |
| Build | CMake 3.16+, GNU Make fallback |

## Usage

### CLI

```
# Discover devices on the network
airdrop discover "My Laptop"

# Send a file (unencrypted)
airdrop send 192.168.1.50 9090 ./photo.jpg

# Send a file (encrypted with passphrase)
airdrop send 192.168.1.50 9090 ./secret.txt mypassword

# Receive files (unencrypted)
airdrop receive

# Receive files (encrypted — passphrase must match sender's)
airdrop receive 9090 ./downloads mypassword
```

### GUI

Build with `-DBUILD_GUI=ON` (requires Qt 6):

```bash
mkdir build && cd build
cmake .. -DBUILD_GUI=ON
make
./airdrop-gui
```

The GUI provides three tabs:
- **Discover** — scan for nearby devices and see them in a list (double-click to auto-fill the IP)
- **Send** — pick a file, enter the target IP/port, optionally encrypt with a passphrase, and watch the progress bar
- **Receive** — start a listener on a port, optionally set a passphrase for decryption, and see incoming transfers logged

## Building

### macOS / Linux (Make)

```bash
make
./airdrop
```

Requires: clang++ or g++ (C++17), OpenSSL dev headers, make

### CMake (cross-platform)

```bash
mkdir build && cd build
cmake ..
make
./airdrop
```

### Windows (MSVC)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
.\Release\airdrop.exe
```

### Qt GUI (optional)

```bash
mkdir build && cd build
cmake .. -DBUILD_GUI=ON
make
./airdrop-gui
```

## Protocol Details

### UDP Discovery Message

```
UAIRDROP:1:<device_name>:<os>:<tcp_port>
```

Broadcast to `255.255.255.255:9091` every 2 seconds.

### TCP File Transfer

**Header** (newline-delimited):

```
UAIRDROP:2:<filename>:<filesize>\n      (unencrypted)
UAIRDROP:6:<filename>:<filesize>\n      (encrypted)
```

**Data**: raw bytes (unencrypted) or `[salt][iv][ciphertext+tag]` (encrypted)

**Response**:

```
UAIRDROP:4:   (ACK — success)
UAIRDROP:5:   (ERR — failure)
```