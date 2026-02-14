CXX = g++
CXXFLAGS = -Wall -std=c++17

BUILD_DIR = build

SERVER_SRC = app/server_app.cpp transport/ReliableSocket.cpp core/UdpSocket.cpp
CLIENT_SRC = app/client_app.cpp transport/ReliableSocket.cpp core/UdpSocket.cpp

SERVER_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRC))

SERVER = $(BUILD_DIR)/server
CLIENT = $(BUILD_DIR)/client

all: $(SERVER) $(CLIENT)

# Compile rule with auto directory creation
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SERVER): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)

run-server:
	./$(SERVER)

run-client:
	./$(CLIENT)
