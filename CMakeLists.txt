cmake_minimum_required(VERSION 3.15)
project(my_muduo)

set(CMAKE_CXX_STANDARD 20)

add_executable(my_muduo
        include/my_net/NonCopyable.h
        src/Logger.cpp
        include/my_net/Logger.h
        src/Timestamp.cpp
        include/my_net/Timestamp.h
        src/InetAddress.cpp
        include/my_net/InetAddress.h
        src/Channel.cpp
        include/my_net/Channel.h
        src/TcpServer.cpp
        include/my_net/TcpServer.h
        src/EventLoop.cpp
        include/my_net/EventLoop.h
        src/Poller.cpp
        include/my_net/Poller.h
        src/DefaultPoller.cpp
        src/EPollPoller.cpp
        include/my_net/EPollPoller.h
        src/CurrentThread.cpp
        include/my_net/CurrentThread.h
        include/my_net/SysHeadFile.h
        src/Thread.cpp
        include/my_net/Thread.h
        src/EventLoopThread.cpp
        include/my_net/EventLoopThread.h
        src/EventLoopThreadpool.cpp
        include/my_net/EventLoopThreadpool.h
        src/Socket.cpp
        include/my_net/Socket.h
        src/Acceptor.cpp
        include/my_net/Acceptor.h
        src/TcpConnection.cpp
        include/my_net/TcpConnection.h
        include/my_net/Callbacks.h
        src/Buffer.cpp
        include/my_net/Buffer.h
        test/test.cpp
)