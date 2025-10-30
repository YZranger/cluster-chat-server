# cluster-chat-server
集群聊天服务器，以nginx为负载均衡器，redis为中间件（消息队列），mysql做持久化存储；以muduo库作为网络模块，在设计上将网络模块，业务模块，数据模块（设计ORM类）分开，层次清晰
