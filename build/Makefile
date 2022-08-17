CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = ../code/log/*.cpp ../code/timer/*.cpp \
       ../code/http/*.cpp ../code/server/*.cpp \
       ../code/buffer/*.cpp ../code/main.cpp \
	   ../code/config/*.cpp ../code/epoll/*.cpp ../code/sqlconnpool/*.cpp 

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient -ljsoncpp

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




