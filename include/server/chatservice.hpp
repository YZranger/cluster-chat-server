#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>

#include "UserModel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 这个函数对象签名和我们在chatserver的onMessage的分发服务分发的东西是一样的
using MsgHandler = std::function<void(const TcpConnectionPtr& conn
                                    , json &js
                                    , Timestamp time)>;

// 聊天服务器业务类   
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();
    // 处理登录业务
    void login(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 服务器异常，业务重置方法
    void reset();
    // 添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 处理注销业务
    void logout(const TcpConnectionPtr& conn, json &js, Timestamp time);
    // 上报消息的回调
    void handleRedissubscribeMessage(int userid, string msg);
private:
    ChatService();

    // 存储消息id和对应的业务处理方法
    unordered_map<int, MsgHandler> handlerMap_;

    // 数据操作类对象
    UserModel userModel_;
    offlineMesgModel offlineMsgModel_;
    FriendModel friendModel_;
    GroupModel groupModel_;

    // 定义互斥锁，保证userConn的线程安全
    mutex connMutex;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> userConnMap_;

    // redis操作对象
    Redis redis_;
};

#endif
