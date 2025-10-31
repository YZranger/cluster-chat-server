#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 控制主菜单聊天页面程序
bool isMainMenuRunning = false;

// 用于读写线程的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr << "command invalid! example:./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if(-1 == connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
    }

    // 初始化读写线程通信的信号量
    sem_init(&rwsem, 0, 0);

    thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // main线程用于接受客户端用户输入，发送数据
    for(;;)
    {
        // 显示首页面的菜单 登录、注册、退出 
        std::cout << "============" << endl;
        std::cout << "1. login" << endl;
        std::cout << "2. register" << endl;
        std::cout << "3. quit" << endl;
        std::cout << "============" << endl;
        std::cout << "choice:" ;
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1:
        {
            int id = 0;
            char pwd[50] = {0};
            std::cout << "userid";
            cin >> id;
            cin.get(); 
            std::cout << "userpassword:",
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            g_isLoginSuccess = false;
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            
            if(len == -1)
            {
                cerr << "send login msg error: " << request << endl; 
            }
            else
            {
                if(g_isLoginSuccess)
                {
                    sem_wait(&rwsem);
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }  
            }
            break;
        }
        case 2:
        {
            char name[50] = {0};
            char pwd[50] = {0};

            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if(len == -1)
            {
                cerr << "send reg msg error: " << request << endl; 
            }
            else
            {
                sem_wait(&rwsem);
            }
            break;
        }
        case 3:
        {
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        }
        default:
        {
            cerr << "invalid input!" << endl;
        }
        }
    }

    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "============login user============" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << "name:" << g_currentUser.getName() << endl;
    cout << "------------friend------------" << endl;
    if(!g_currentUserFriendList.empty())
    {
        for(auto &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "------------group list------------" << endl;
    if(!g_currentUserFriendList.empty())
    {
        for(auto &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for(auto &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << user.getRole() << endl;
            }
        }
    }
    cout << "=====================================" << endl;
}

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void logout(int, string);


// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"logout", "注销，格式logout"},
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", logout},
};


void doLoginResponse(json &js)
{
    if(0 != js["errno"]) // 登录失败
    {
        cerr << js["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else // 登录成功
    {   
        g_isLoginSuccess = true;
        // 记录当前用户的id和name
        g_currentUser.setId(js["id"]);
        g_currentUser.setName(js["name"]);

        // 记录当前用户的好友列表信息
        g_currentUserFriendList.clear();
        if(js.contains("friends"))
        {
            vector<string> vec = js["friends"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"]);
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        g_currentUserGroupList.clear();
        if(js.contains("group"))
        {
            vector<string> vec1 = js["groups"];
            for(string &groupstr : vec1)
            {
                json grpjs = json::parse(groupstr);
                Group group;
                group.setId(grpjs["id"]);
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for(string &userstr : vec2)
                {
                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"]);
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息，个人聊天信息或者群组消息
        if(js.contains("offlinemsg"))
        {
            vector<string> vec = js["offlinemsg"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                if(ONE_CHAT_MSG == js["msgid"])
                {
                    cout << js["time"] << "[" << js["id"] << "]" << js["name"]
                        << "said: " << js["msg"] << endl; 
                }
                
                if(GROUP_CHAT_MSG == js["msgid"])
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"] << "[" << js["id"]
                    << "said: " << js["msg"] << endl; 
                }
            }
        }
    }
}

// 处理注册的响应逻辑
void doRegResponse(json &responsejs)
{
    if(0 != responsejs["errno"]) // 注册失败
    {
        cerr << "is already exist, register error!" << endl;
    }
    else // 注册成功
    {
        cout << "register success, userid is" << responsejs["id"]
        << ", do not forget it!" << endl;
    }
}

void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0); // 阻塞
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json对象
        json js = json::parse(buffer);
        if(ONE_CHAT_MSG == js["msgid"])
        {
            cout << js["time"] << "[" << js["id"] << "]" << js["name"]
                << "said: " << js["msg"] << endl; 
            continue;
        }
        
        if(GROUP_CHAT_MSG == js["msgid"])
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"] << "[" << js["id"]
            << "said: " << js["msg"] << endl;
            continue;
        }

        if(LOGIN_MSG == js["msgid"])
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);
            continue;
        }

        if(REG_MSG_ACK == js["msgid"])
        {
            doRegResponse(js);
            sem_post(&rwsem);
            continue;
        }
    }
}

void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if(-1 == idx)
        {
            command =  commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }

}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %-2d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

    return std::string(date);
}

void help(int fd, string str)
{
    cout << "show command list >>> " << endl;
    for(auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}

void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}

void logout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(-1 == len)
    {
        cerr << "send logout msg error -> " << buffer << endl;
        return;
    }
    isMainMenuRunning = false;
}