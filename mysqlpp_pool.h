/**
 * @author rench
 * @email finyren@163.com
 * @create date 2020-04-15 19:33:30
 * @modify date 2020-04-15 19:33:30
 * @desc [mysqlpp_conn连接池管理以及屏蔽数据库异步连接状态机]
 */

#ifndef __mysql_plus_plus_h_
#define __mysql_plus_plus_h_

// #include "mysqlpp/mysqlpp.h"
// #include "mysql++/mysql_conn.h"
// #include "mysqlpp/mysqlpp_result.h"

#include <vector>
#include <string>

static const int def_max_idle = 120;
static const int def_max_conn = 20;

struct event_base;

class mysqlpp_conn; 

// every thread shoule hava a mysqlpp instance and a evloop
class mysqlpp_pool {
public:
    mysqlpp_pool(struct event_base *evloop, 
        const std::string &host,
        const int port,
        const std::string &user,
        const std::string &passwd,
        const std::string &dbname,
        int max_idle = def_max_idle,
        int max_conn = def_max_conn);

    ~mysqlpp_pool();

    static void init_library(int argc, const char **argv);  // for init mysql library

    void set_evloop(struct event_base *evloop) { // for init event loop
        _evloop = evloop;
    }

    mysqlpp_conn *get_connection();

    int get_all_active();
    int get_pool_active();
    int get_available();

    void add_connection(mysqlpp_conn *conn);

private:
    struct event_base *_evloop; // for async mysql operation

    std::string _host;
    int _port;
    std::string _user;
    std::string _passwd;
    std::string _dbname;

    int _max_idle;
    int _max_conn;

    int _all;

    std::vector<mysqlpp_conn *> _conns;
};


#endif
