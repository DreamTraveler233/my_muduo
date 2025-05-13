cmake_minimum_required(VERSION 3.15)
project(my_muduo)

set(CMAKE_CXX_STANDARD 20)

add_executable(my_muduo
        include/net/NonCopyable.h
        src/Logger.cpp
        include/net/Logger.h
        src/Timestamp.cpp
        include/net/Timestamp.h
        src/InetAddress.cpp
        include/net/InetAddress.h
        src/Channel.cpp
        include/net/Channel.h
        src/TcpServer.cpp
        include/net/TcpServer.h
        src/EventLoop.cpp
        include/net/EventLoop.h
        src/Poller.cpp
        include/net/Poller.h
        src/DefaultPoller.cpp
        src/EPollPoller.cpp
        include/net/EPollPoller.h
        src/CurrentThread.cpp
        include/net/CurrentThread.h
        include/net/SysHeadFile.h
        src/Thread.cpp
        include/net/Thread.h
        src/EventLoopThread.cpp
        include/net/EventLoopThread.h
        src/EventLoopThreadpool.cpp
        include/net/EventLoopThreadpool.h
        src/Socket.cpp
        include/net/Socket.h
        src/Acceptor.cpp
        include/net/Acceptor.h
        src/TcpConnection.cpp
        include/net/TcpConnection.h
        include/net/Callbacks.h
        src/Buffer.cpp
        include/net/Buffer.h
        test/test.cpp
)