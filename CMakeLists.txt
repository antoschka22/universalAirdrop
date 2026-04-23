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

# ========== Tests ==========
option(BUILD_TESTS "Build unit and integration tests" ON)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    # Library sources (without main.cpp) for linking into tests
    set(LIB_SOURCES
        src/tcp_server.cpp
        src/tcp_client.cpp
        src/udp_discovery.cpp
        src/file_transfer.cpp
        src/crypto.cpp
    )

    add_executable(test_protocol tests/test_protocol.cpp ${LIB_SOURCES})
    target_include_directories(test_protocol PRIVATE include)
    target_link_libraries(test_protocol PRIVATE GTest::gtest GTest::gtest_main OpenSSL::Crypto)
    if(UNIX)
        target_link_libraries(test_protocol PRIVATE pthread)
    endif()
    add_test(NAME Protocol COMMAND test_protocol)

    add_executable(test_crypto tests/test_crypto.cpp ${LIB_SOURCES})
    target_include_directories(test_crypto PRIVATE include)
    target_link_libraries(test_crypto PRIVATE GTest::gtest GTest::gtest_main OpenSSL::Crypto)
    if(UNIX)
        target_link_libraries(test_crypto PRIVATE pthread)
    endif()
    add_test(NAME Crypto COMMAND test_crypto)

    add_executable(test_file_transfer tests/test_file_transfer.cpp ${LIB_SOURCES})
    target_include_directories(test_file_transfer PRIVATE include)
    target_link_libraries(test_file_transfer PRIVATE GTest::gtest GTest::gtest_main OpenSSL::Crypto)
    if(UNIX)
        target_link_libraries(test_file_transfer PRIVATE pthread)
    endif()
    add_test(NAME FileTransfer COMMAND test_file_transfer)

    add_executable(test_integration tests/test_integration.cpp ${LIB_SOURCES})
    target_include_directories(test_integration PRIVATE include)
    target_link_libraries(test_integration PRIVATE GTest::gtest GTest::gtest_main OpenSSL::Crypto)
    if(UNIX)
        target_link_libraries(test_integration PRIVATE pthread)
    endif()
    add_test(NAME Integration COMMAND test_integration)
endif()