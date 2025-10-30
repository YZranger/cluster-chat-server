#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

class GroupUser : public User
{
public:
    void setRole(string role) { role_ = role; }
    string getRole() { return role_; }
private:
    string role_;
};

#endif