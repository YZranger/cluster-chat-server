#include "chatserver.hpp"
#include "chatservice.hpp"
#include <csignal>
#include <signal.h>
#include <iostream>

using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv) {
    uint16_t port;
    string ip;
    
    if(argc == 1)
    {
        ip = "127.0.0.1";
        port = 6000;
    }
    else if(argc == 3)
    {
        
        port = atoi(argv[2]);
        ip = argv[1];
    }
    else
    {
        cout << "command input error! exp:./ChatService ip port" << endl;
        exit(-1);
    }

    signal(SIGINT, resetHandler);
    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}