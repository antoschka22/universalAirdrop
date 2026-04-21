cmake_minimum_required(VERSION 3.15)
project(UniversalAirdrop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lws2_32")
endif()

add_executable(airdrop
    src/main.cpp
    src/tcp_server.cpp
    src/tcp_client.cpp
    src/udp_discovery.cpp
    src/file_transfer.cpp
)

target_include_directories(airdrop PRIVATE include)

if(WIN32)
    target_link_libraries(airdrop PRIVATE ws2_32 iphlpapi)
endif()

if(UNIX)
    target_link_libraries(airdrop PRIVATE pthread)
endif()