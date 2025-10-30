#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <string>

using namespace muduo::net;
using namespace muduo;

// TODO:这里没有解决TCP协议拆包和粘包的问题

// 聊天服务器的主类
class ChatServer {
public:
    // 初始化聊天服务器对象
    ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const std::string& nameArg);

    // 启动服务
    void start();
private:
    // 上报连接相关信息的回调函数
    void onConnection(const TcpConnectionPtr&);

    // 上报读写信息相关信息的回调函数
    void onMessage(const TcpConnectionPtr&,
                        Buffer*,
                        Timestamp);

    TcpServer server_; // 组合的muduo库，实现服务器功能的类对象
    EventLoop* loop_;  // 指向时间循环对象的指针
};

#endif