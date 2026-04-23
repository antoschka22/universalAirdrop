CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I include
LDFLAGS = -lpthread

# OpenSSL
OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null || echo /usr)
CXXFLAGS += -I$(OPENSSL_PREFIX)/include
LDFLAGS += -L$(OPENSSL_PREFIX)/lib -lcrypto

# Google Test
GTEST_PREFIX := $(shell brew --prefix googletest 2>/dev/null || echo /usr)
GTEST_CXXFLAGS = -I$(GTEST_PREFIX)/include
GTEST_LDFLAGS = -L$(GTEST_PREFIX)/lib -lgtest -lgtest_main -lpthread

# Source files
SRCS = src/main.cpp src/tcp_server.cpp src/tcp_client.cpp src/udp_discovery.cpp src/file_transfer.cpp src/crypto.cpp
LIB_SRCS = src/tcp_server.cpp src/tcp_client.cpp src/udp_discovery.cpp src/file_transfer.cpp src/crypto.cpp
TARGET = airdrop

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Test targets
test_protocol: tests/test_protocol.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -o $@ $^ $(LDFLAGS) $(GTEST_LDFLAGS)

test_crypto: tests/test_crypto.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -o $@ $^ $(LDFLAGS) $(GTEST_LDFLAGS)

test_file_transfer: tests/test_file_transfer.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -o $@ $^ $(LDFLAGS) $(GTEST_LDFLAGS)

test_integration: tests/test_integration.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -o $@ $^ $(LDFLAGS) $(GTEST_LDFLAGS)

test: test_protocol test_crypto test_file_transfer test_integration
	./test_protocol
	./test_crypto
	./test_file_transfer
	./test_integration

clean:
	rm -f $(TARGET) test_protocol test_crypto test_file_transfer test_integration

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run test