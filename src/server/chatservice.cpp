#include "chatservice.hpp"
#include "UserModel.hpp"
#include "public.hpp"

#include <muduo/base/Logging.h>
#include <unistd.h>

using namespace placeholders;

//  获取单例对象的接口函数
ChatService* ChatService::instance() 
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的handler回调
ChatService::ChatService()
{
    handlerMap_.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    handlerMap_.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    handlerMap_.insert({ONE_CHAT_MSG, bind(&ChatService::oneChat, this, _1, _2, _3)});
    handlerMap_.insert({ADD_FRIEND, bind(&ChatService::addFriend, this, _1, _2, _3)});
    handlerMap_.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup, this, _1, _2, _3)});
    handlerMap_.insert({LOGOUT_MSG, bind(&ChatService::logout, this, _1, _2, _3)});
    handlerMap_.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup, this, _1, _2, _3)});
    handlerMap_.insert({GROUP_CHAT_MSG, bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if(redis_.connect())
    {
        // 设置上报消息的回调
        redis_.init_notify_handler(bind(&ChatService::handleRedissubscribeMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid 没有对应的事件处理回调
    auto it = handlerMap_.find(msgid);
    if(it == handlerMap_.end()) 
    {
        return [=](const TcpConnectionPtr& conn, json &js, Timestamp time) {
            LOG_ERROR << "msgid:" << msgid << "can not find handler";
        };
    }
    else
    {
        return handlerMap_[msgid];
    }
}

void ChatService::login(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];

    User user = userModel_.query(id);
    if(user.getId() == id && user.getPassword() == pwd)
    {
        if(user.getState() == "online")
        {
            // 该账号已经登录过了
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录，请重新输入账号";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功 
            // 更新用户状态信息state offline -> online
            user.setState("online");
            userModel_.updateState(user);

            // 记录用户的连接信息
            {
                lock_guard<mutex> lock(connMutex);
                userConnMap_.insert({id, conn});
            }

            // id用户登录成功后,向redis订阅channel(id)
            redis_.subscribe(id);
            
            // 回复客户端
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有给自己的离线消息
            vector<string> vec = offlineMsgModel_.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息以后，删除数据库里面存储的离线消息
                offlineMsgModel_.remove(id);
            }

            // 查询用户的好友信息并返回
            vector<User> userVec = friendModel_.query(id);
            vector<string> userinfo;
            if(!userVec.empty())
            {
                for(auto &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    userinfo.push_back(js.dump());
                }
                response["friends"] = userinfo;
            }

            vector<Group> groupuserVec = groupModel_.queryGroups(id);
            if(!groupuserVec.empty())
            {    
                vector<string> groupV;
                for(Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    
                    for(GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else 
    {
        // 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理注册业务name password
void ChatService::reg(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = userModel_.insert(user);

    if(state)
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{   
    User user;
    {
        lock_guard<mutex> lock(connMutex);
        for(auto it = userConnMap_.begin(); it != userConnMap_.end(); it++)
        {
            if(it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    redis_.unsubscribe(user.getId());

    // 更新用户的状态信息
    if(user.getId() != -1)
    {
        user.setState("offline");
        userModel_.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int toid = js["toid"];

    {
        lock_guard<mutex> lock(connMutex);
        auto it = userConnMap_.find(toid);

        if(it != userConnMap_.end())
        {
            // toid在线（本机），转发消息
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线（是否在其他服务器登录）
    User user = userModel_.query(toid);
    if(user.getState() == "online")
    {
        redis_.publish(toid, js.dump());
        return;
    }

    // 不在线，存储离线消息
    offlineMsgModel_.insert(toid, js.dump());
}

void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    userModel_.resetState();
}

// 添加好友业务msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int friendid = js["friendid"];

    if(userConnMap_.find(userid) == userConnMap_.end())
    {
        LOG_ERROR << __FILE__ << __LINE__ << __FUNCTION__ << "failed: you should login first!";
        return;
    }

    // 存储好友信息
    friendModel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"];
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建爱你的群组信息
    Group group(-1, name, desc);
    if(groupModel_.createGroup(group))
    {
        // 存储群组创建人信息
        groupModel_.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int groupid = js["groupid"];
    groupModel_.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int groupid = js["groupid"];

    vector<int> useridVec = groupModel_.queryGroupUsers(userid, groupid);
    for(int id : useridVec)
    {
        lock_guard<mutex> lock(connMutex);
        auto it = userConnMap_.find(id);
        if(it != userConnMap_.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {   
            // 查询toid是否在线
            User user = userModel_.query(id);
            if(user.getState() == "online")
            {
                redis_.publish(id, js.dump());
            }
            // 存储离线消息
            offlineMsgModel_.insert(id, js.dump());
        }
    }
}

void ChatService::logout(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"];
    {
        lock_guard<mutex> lock(connMutex);
        auto it = userConnMap_.find(userid);
        if(it != userConnMap_.end())
        {
            userConnMap_.erase(it);
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    userModel_.updateState(user);

}

void ChatService::handleRedissubscribeMessage(int userid, string msg)
{
    // TODO:注意，这里不应该直接发出去，而是用tcpconnection的runinloop方法，否则会导致发出的消息错乱
    // 而且这里锁的粒度太大，一旦send阻塞，马上成为新能瓶颈
    lock_guard<mutex> lock(connMutex);
    auto it = userConnMap_.find(userid);
    if(it != userConnMap_.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储接收对象的离线消息
    offlineMsgModel_.insert(userid, msg);
}