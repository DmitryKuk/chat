# Makefile
# Author: Dmitry Kukovinets (d1021976@gmail.com)

# Server
SERVER_SRCS=server.cpp message.cpp
SERVER_TARGET=server

# Client
CLIENT_SRCS=client.cpp message.cpp
CLIENT_TARGET=client


SERVER_OBJS=$(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS=$(CLIENT_SRCS:.cpp=.o)

GPP=g++ -std=c++0x -Wall

# Цели
.PHONY: all clear

all: $(SERVER_TARGET) $(CLIENT_TARGET)

clear:
	rm $(SERVER_TARGET) $(CLIENT_TARGET) $(SERVER_OBJS) $(CLIENT_OBJS)


$(SERVER_TARGET): $(SERVER_OBJS)
	$(GPP) -o $@ $^

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(GPP) -o $@ $^

# Неявные преобразования
%.o: %.cpp
	$(GPP) -o $@ -c $<
