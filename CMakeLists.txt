cmake_minimum_required(VERSION 3.16)
project(UniversalAirdrop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common sources
set(CLI_SOURCES
    src/main.cpp
    src/tcp_server.cpp
    src/tcp_client.cpp
    src/udp_discovery.cpp
    src/file_transfer.cpp
    src/crypto.cpp
)

set(GUI_SOURCES
    src/gui/main_window.cpp
    src/gui/main_window.ui
    src/gui_main.cpp
    src/tcp_server.cpp
    src/tcp_client.cpp
    src/udp_discovery.cpp
    src/file_transfer.cpp
    src/crypto.cpp
)

# ========== CLI Target ==========
add_executable(airdrop ${CLI_SOURCES})
target_include_directories(airdrop PRIVATE include)

# Platform-specific linking
if(WIN32)
    target_link_libraries(airdrop PRIVATE ws2_32 iphlpapi)
endif()
if(UNIX)
    target_link_libraries(airdrop PRIVATE pthread)
endif()

# OpenSSL (required)
find_package(OpenSSL REQUIRED)
target_include_directories(airdrop PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(airdrop PRIVATE OpenSSL::Crypto)

# ========== GUI Target (optional, requires Qt6) ==========
option(BUILD_GUI "Build the Qt6 GUI application" OFF)

if(BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    add_executable(airdrop-gui ${GUI_SOURCES})
    target_include_directories(airdrop-gui PRIVATE include)

    if(WIN32)
        target_link_libraries(airdrop-gui PRIVATE ws2_32 iphlpapi)
    endif()
    if(UNIX)
        target_link_libraries(airdrop-gui PRIVATE pthread)
    endif()

    target_link_libraries(airdrop-gui PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Network OpenSSL::Crypto)
endif()