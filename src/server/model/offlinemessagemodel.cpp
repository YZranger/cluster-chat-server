#include "offlinemessagemodel.hpp"
#include "db.h"

 // 存储用户的离线消息
void offlineMesgModel::insert(int userid, string msg)
{
     // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into OfflineMessage values (%d, '%s')", userid, msg.c_str()); 

    // 使用mysql c API
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除用户的离线消息
void offlineMesgModel::remove(int userid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid=%d", userid); 

    // 使用mysql c API
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
vector<string> offlineMesgModel::query(int userid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where id = %d", userid);

    // 使用mysql c API进行查询
    vector<string> vec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row; 
            // 把userid用户的所有离线消息放入vec中返回
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }

    return vec;
}