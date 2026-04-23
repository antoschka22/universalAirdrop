CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I include
LDFLAGS = -lpthread

# OpenSSL
OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null || echo /usr)
CXXFLAGS += -I$(OPENSSL_PREFIX)/include
LDFLAGS += -L$(OPENSSL_PREFIX)/lib -lcrypto

SRCS = src/main.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_discovery.cpp src/file_transfer.cpp src/crypto.cpp
TARGET = airdrop

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run