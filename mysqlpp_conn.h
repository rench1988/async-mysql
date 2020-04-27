/**
 * @author rench
 * @email finyren@163.com
 * @create date 2020-04-16 09:09:36
 * @modify date 2020-04-16 09:09:36
 * @desc [description]
 */
#ifndef __mysql_conn_h__
#define __mysql_conn_h__

#include <string>
#include <map>
#include "mysql/mysql.h"
#include "mysqlpp_pool.h"

/*
 mysql_close/mysql_stmt_close这两个api只是简单的发送COM_QUIT/COM_STMT_CLOSE给server, 并且不等待响应，所以几乎是不会阻塞的(除非写buffer满).
 因此在特殊情况下, 也会使用阻塞版本的api.
*/

class mysqlpp_conn;

typedef bool (*user_callback)(mysqlpp_conn *conn, void *argument); // user's callback function

typedef void (*manager_callback)(mysqlpp_conn *conn);

struct event;
struct event_base;

class mysqlpp_pool;

typedef struct param_s {
    union {
        double real;
        int integer;
        long long llong;
        MYSQL_TIME timestamp;
    } type;
    
    unsigned long length;
} param_t;

class mysqlpp_bind {
public:
    mysqlpp_bind(int size);
    ~mysqlpp_bind();

    bool set_string(int parameterIndex, const char *x);
    bool set_int(int parameterIndex, int x);
    bool set_llong(int parameterIndex, long long x);
    bool set_double(int parameterIndex, double x);
    bool set_timestamp(int parameterIndex, time_t x);
    bool set_blob(int parameterIndex, const void *x, int size);

    bool bind_stmt(MYSQL_STMT *stmt);

private:
    param_t *_params;
    int _size;
    MYSQL_BIND *_bind;
};

typedef struct column_s {
    char *buffer;
    my_bool is_null;
    MYSQL_FIELD *field;
    unsigned long real_length;
} column_t;

class mysqlpp_result {
public:
    mysqlpp_result(int columnCount, MYSQL_RES *meta, MYSQL_STMT *stmt);
    ~mysqlpp_result();

    int bind_stmt_result();

    int get_index(const char *name);

    const char *get_string(int columnIndex);
    const void *get_blob(int columnIndex, int &size);
    int get_int(int columnIndex, bool &is_null);
    long long get_llong(int columnIndex, bool &is_null);
    double get_double(int columnIndex, bool &is_null);
    // time_t get_timestamp(int columnIndex);

    const char *get_string_by_name(const char *columnName);
    const void *get_blob_by_name(const char *columnName, int &size);
    int get_int_by_name(const char *columnName, bool &is_null);
    long long get_llong_by_name(const char *columnName, bool &is_null);
    double get_double_by_name(const char *columnName, bool &is_null);
    // time_t get_timestamp_by_name(const char *columnName);

private:
    void _ensure_capacity(int index);

    bool _needRebind;
    int _columnCount;
    int _currentRow;
    MYSQL_RES *_meta;
    MYSQL_BIND *_bind;
    MYSQL_STMT *_stmt;

    column_t *_columns;
};

class mysqlpp_conn {
public:

    typedef void (*state_done)(mysqlpp_conn *conn);
    typedef void (*state_machine)(int sockfd, short which, void *v);  // add to event loop
    
    enum Estatus {
        CONNECT_START,
        CONNECT_WAITING,
        CONNECT_DONE,

        PREPARE_START,
        PREPARE_WAITING,
        PREPARE_DONE,

        EXECUTE_START,
        EXECUTE_WAITING,
        EXECUTE_DONE,

        STMT_FETCH_START,
        STMT_FETCH_WAITING,
        STMT_FETCH_DONE,

        CLOSE_STMT_START,
        CLOSE_STMT_WAITING,
        CLOSE_STMT_DONE,

        QUERY_START,
        QUERY_WAITING,
        QUERY_RESULT_READY,

        FETCH_ROW_START,
        FETCH_ROW_WAITING,
        FETCH_ROW_RESULT_READY,

        CLOSE_START,
        CLOSE_WAITING,
        CLOSE_DONE,

        NOTHING
    };

public:
    static void init_library(int argc, const char **argv, char **groups);

    void close();  // return back to connection pool or close it

    bool is_available() {
        return _available;
    }

    void set_available(bool available) {
        _available = available;
    }

    bool failed() {
        return _failed;
    }

    const char *error();

    void query(std::string &sql);
    void prepare(std::string &sql);
    void execute();
    void execute_query();

    bool result_eof() {
        return _eof;
    }

    uint64_t affected_rows();
    uint64_t insert_id();

    void set_user_callback(const user_callback &cb) {
        _user_callback = cb;
    }

    void set_user_argument(void *argument) {
        _user_argument = argument;
    }

    void *get_user_argument() {
        return _user_argument;
    }

    mysqlpp_result *get_exec_result() {
        return _exec_result;
    }

    mysqlpp_bind *get_exec_bind() {
        return _bind;
    }

    int get_column_count() {
        return _columns;
    }

    char **get_column_content() {
        return _row;
    }

private:
    mysqlpp_conn(struct event_base *loop,  
            const std::string &host, 
            const int port, 
            const std::string &user, 
            const std::string &passwd, 
            const std::string &dbname,
            mysqlpp_pool *pp);

    ~mysqlpp_conn();

    // 只有get_connection和add_connection才可以创建和释放连接
    friend mysqlpp_conn *mysqlpp_pool::get_connection();
    friend void mysqlpp_pool::add_connection(mysqlpp_conn *conn);
    friend mysqlpp_pool::~mysqlpp_pool();

    static void conn_state_machine(int sockfd, short event, void *v);
    static void query_state_machine(int sockfd, short event, void *v);
    static void fetch_state_machine(int sockfd, short event, void *v);
    static void close_state_machine(int sockfd, short event, void *v);
    static void prepare_state_machine(int sockfd, short event, void *v);
    static void close_stmt_state_machine(int sockfd, short event, void *v);    
    static void stmt_fetch_state_machine(int sockfd, short event, void *v);
    static void execute_state_machine(int sockfd, short event, void *v);

    static void close_callback(int sockfd, short event, void *v);

    void unset_callback();

    void cleanup();

    void add_to_connection_pool();

    void set_def_option();

    void connect();

    void next_event(Estatus new_status, int status);

    void conn_done();
    void query_done();
    int  row_done();
    void prepare_done();
    void execute_done();
    // void execute_query_done();
    bool stmt_fetch_done();
    void close_done();
    void close_stmt_done();

    void detach_event();
    void free_result();  // 必须读完在free_result, 否则会阻塞
    void free_stmt_blocking();

    static int mysql_status(short event);

private:
    mysqlpp_result *_exec_result;

    user_callback _user_callback;

    void *_user_argument;

    bool _failed;

    bool _eof;

    bool _closing;

    std::string _sb;

    mysqlpp_pool *_pp;

    struct event *_event;

    bool _attached;
    bool _connected;
    bool _prepared;

    bool _available;

    MYSQL _mysql;
    MYSQL *_ret;
    MYSQL_RES *_result;
    MYSQL_STMT *_stmt;
    MYSQL_ROW _row;

    mysqlpp_bind *_bind;

    state_machine _state_machine;

    struct event_base *_loop;

    int _columns;

    bool _exec_flag;

    std::string _host;
    std::string _user;
    std::string _passwd;
    std::string _dbname;
    int _port;

    int _err;

    my_bool _e; // for mysql_stmt_close_start and so on

    std::string _sql;


    Estatus _status;
};

#endif
