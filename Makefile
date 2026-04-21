CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I include
LDFLAGS = -lpthread

SRCS = src/main.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_discovery.cpp src/file_transfer.cpp
TARGET = airdrop

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run