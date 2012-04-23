/* 
 * File:   UidToUser.cpp
 * Author: lior
 * 
 * Created on April 19, 2012, 3:48 PM
 */

#include <pwd.h>
#include <ModuleLogger.h>
#include "UidToUser.h"
#include <sstream>
#include <sys/time.h>
#define U2U_DEFAULT_TTL  (500)
UidToUser::UidToUser() {
    _ttl_sec = U2U_DEFAULT_TTL;
    mlog_registerModule("u2u", "Uid to User name cache", "u2u");
    _users_map.clear();
}

UidToUser::UidToUser(const UidToUser& orig) {

}

UidToUser::~UidToUser() {
}

void UidToUser::set_ttl(int ttl_sec) {
    if(ttl_sec >= 0) {
        _ttl_sec = ttl_sec;
    }
    
}


std::string UidToUser::get_user(int uid) {
    if (uid < 0) {
        return std::string("not-leagal");
    }

    if(_users_map.count(uid) == 0) {
        user_info ui;
        ui._uid = uid;
        gettimeofday(&ui._update_time, NULL);
        ui._user = _get_user(uid);
        _users_map[uid] = ui;
        return ui._user;
    }
    
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    if( (curr_time.tv_sec - _users_map[uid]._update_time.tv_sec) > _ttl_sec) {
            gettimeofday(&_users_map[uid]._update_time, NULL);
            _users_map[uid]._user = _get_user(uid);
    }
    return _users_map[uid]._user;
}

/* Getting the user name according to the uid of the user. 
 * If the user is not found the uid is returned as the content of the string
 * If the uid is < 0 the word not-leagal is returned
 */
std::string UidToUser::_get_user(int uid) {
    if (uid < 0) {
        return std::string("not-leagal");
    }

    passwd *pw;

    pw = getpwuid(uid);
    if (pw) {
        mlog_bn_dg("u2u", "Found name [%s] to uid [%d]\n", pw->pw_name, uid );
        return (std::string(pw->pw_name));

    }
    mlog_bn_dg("u2u", "Uid [%d] can not be translated to \n", pw->pw_name, uid );
        
    std::stringstream str_stream;
    str_stream << uid;
    return str_stream.str();
}

