/**
 * @author rench
 * @email finyren@163.com
 * @create date 2020-04-16 09:29:00
 * @modify date 2020-04-16 09:29:00
 * @desc [description]
 */
#include "mysqlpp_pool.h"
#include "mysqlpp_conn.h"

static const char *groups[]= {"mysql++", NULL};

mysqlpp_pool::mysqlpp_pool(struct event_base *evloop,
                const std::string &host,
                const int port,
                const std::string &user,
                const std::string &passwd,
                const std::string &dbname,
                int max_idle,
                int max_conn) 
    : _evloop(evloop),
      _host(host),
      _port(port),
      _user(user),
      _passwd(passwd),
      _dbname(dbname),
      _max_idle(max_idle),
      _max_conn(max_conn) {
}

mysqlpp_pool::~mysqlpp_pool() {
    for (unsigned int i = 0; i < _conns.size(); i++) {
        delete _conns[i];
    }

    _all = 0;
}

void mysqlpp_pool::init_library(int argc, const char **argv) {
    mysqlpp_conn::init_library(argc, argv, (char **)groups);
}

mysqlpp_conn *mysqlpp_pool::get_connection() {
    for (unsigned int i = 0; i < _conns.size(); i++) {
        if (!_conns[i]->is_available()) {
            continue;
        }

        _conns[i]->set_available(false);
        return _conns[i];
    }

    _all++;

    return new mysqlpp_conn(_evloop, _host, _port, _user, _passwd, _dbname, this);
}

int mysqlpp_pool::get_all_active() {
    return _all;
}

int mysqlpp_pool::get_pool_active() {
    int active = 0;

    for (unsigned int i = 0; i < _conns.size(); i++) {
        if (_conns[i]->is_available()) {
            continue;
        }

        active++;
    }

    return active;
}

int mysqlpp_pool::get_available() {
    int available = 0;

    for (unsigned int i = 0; i < _conns.size(); i++) {
        if (_conns[i]->is_available()) {
            available++;
        }
    }

    return available;
}

void mysqlpp_pool::add_connection(mysqlpp_conn *conn) {
    if (_conns.size() > (unsigned int)_max_conn || !conn->_connected) {
        _all--;
        delete conn;

        return;
    }

    conn->set_available(true);
    _conns.push_back(conn);
}
