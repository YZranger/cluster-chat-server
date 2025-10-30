#include "UserModel.hpp"
#include "db.h"
#include <mysql/mysql.h>

using namespace std;

bool UserModel::insert(User &user)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values ('%s', '%s', '%s')"
        , user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str()); // TODO:有sql注入的风险，应该用参数查询的方式组装sql语句

    // 使用mysql c API进行插入
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

User UserModel::query(int id)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d", id);

    // 使用mysql c API进行查询
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if(row != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPassword(row[2]);
                user.setState(row[3]);

                // 释放res的内存资源
                mysql_free_result(res);
                return user;
            }
        }
    }

    return User();
}

bool UserModel::updateState(User user)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "update User set state = '%s' where id = %d", 
            user.getState().c_str(), user.getId());

    // 使用mysql c API进行更新
    MySQL mysql;
    if(mysql.connect())
    {
      if(mysql.update(sql))
      {
        return true;
      }
    }        

    return false;
}

void UserModel::resetState()
{
    // 1. 组装sql语句
    char sql[1024] = {"update User set state = 'offline' where state = 'online'"};

    // 使用mysql c API进行更新
    MySQL mysql;
    if(mysql.connect())
    {
      mysql.update(sql);
    }        
}