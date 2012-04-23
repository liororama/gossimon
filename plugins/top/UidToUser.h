/* 
 * File:   UidToUser.h
 * Author: lior
 *
 * Created on April 19, 2012, 3:48 PM
 */

#ifndef UIDTOUSER_H
#define	UIDTOUSER_H

#include <string>
#include <map>
#include <time.h>
struct user_info {
    int            _uid;
    std::string    _user;
    struct timeval _update_time;
};

class UidToUser {
public:
    UidToUser();
    UidToUser(const UidToUser& orig);
    virtual ~UidToUser();
    
    void set_ttl(int ttl_sec);
    std::string get_user(int uid);

private:
    std::string _get_user(int uid);
    
    int  _ttl_sec;
    std::map<int, user_info> _users_map;
    

};

#endif	/* UIDTOUSER_H */

