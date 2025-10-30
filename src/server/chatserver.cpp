#include "chatserver.hpp"
#include "chatservice.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <nlohmann/json_fwd.hpp>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const std::string& nameArg) 
            :server_(loop, listenAddr, nameArg)
            ,loop_(loop)
{
    // 注册连接回调
    server_.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
    // 注册消息回调
    server_.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    server_.setThreadNum(4);
}

void ChatServer::start() 
{
    server_.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn) 
{
    if(!conn->connected()) {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr& conn,
                        Buffer* buffer,
                        Timestamp time) 
{
    string buf = buffer->retrieveAllAsString();

    // 数据的反序列化
    json js = json::parse(buf);
    // 达到的目的：完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"]获取=>业务handler=>conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"]);
    msgHandler(conn, js, time); // TODO:i/o线程不应该处理业务逻辑
}