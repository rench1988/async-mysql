#include <iostream>
#include <string>
#include <event2/event.h>
#include "mysqlpp_pool.h"
#include "mysqlpp_conn.h"


class db_test_task {
public:
    db_test_task();
    ~db_test_task();

    static bool query_callback1_1(mysqlpp_conn *conn, void *argument);
    static bool query_callback1_2(mysqlpp_conn *conn, void *argument);

    static bool query_callback2_1(mysqlpp_conn *conn, void *argument);
    static bool query_callback2_2(mysqlpp_conn *conn, void *argument);
    static bool query_callback2_3(mysqlpp_conn *conn, void *argument);
    static bool query_callback2_4(mysqlpp_conn *conn, void *argument);

    mysqlpp_conn *conn;
};

db_test_task::db_test_task() {

}

db_test_task::~db_test_task() {

}

bool db_test_task::query_callback1_2(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }

    std::cout << conn->affected_rows() << " " << conn->insert_id() << std::endl;

    return false;  // need next row    
}

bool db_test_task::query_callback1_1(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }    

    if (conn->result_eof()) {
        std::string sql = "insert into z1 values(100)";

        conn->set_user_callback(&db_test_task::query_callback1_2);
        conn->query(sql);
        return true;
    }

    std::cout << conn->get_column_count() << " " << conn->get_column_content()[0] << std::endl;

    return false;  // need next row
}

void Test_query(mysqlpp_pool *pp) {
    db_test_task *task = new db_test_task();

    std::string sql = "select * from z1 limit 5";

    mysqlpp_conn *conn = pp->get_connection();
    task->conn = conn;

    conn->set_user_callback(&db_test_task::query_callback1_1);  // 设置用户回调函数
    conn->set_user_argument(task); // 设置用户回调函数参数

    conn->query(sql);
}

bool db_test_task::query_callback2_4(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }

    std::cout << "affected rows: " << conn->affected_rows() << std::endl;

    return true;    
}

bool db_test_task::query_callback2_3(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }

    mysqlpp_bind *bind = conn->get_exec_bind();
    bind->set_int(1, 777);   

    conn->set_user_callback(&db_test_task::query_callback2_4);
    conn->execute();

    return false;     
}

bool db_test_task::query_callback2_2(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }

    if (conn->result_eof()) {
        //conn->close();
        std::string sql = "insert into z1 values(?)";

        conn->set_user_callback(&db_test_task::query_callback2_3);
        conn->prepare(sql);

        return true;
    }

    mysqlpp_result *result = conn->get_exec_result();

    bool is_null;

    std::cout << "********************* " << result->get_int(1, is_null) << std::endl;

    return false;    
}

bool db_test_task::query_callback2_1(mysqlpp_conn *conn, void *argument) {
    db_test_task *t = (db_test_task *)argument;

    (void)t;

    if (conn->failed()) {
        std::cout << conn->error() << std::endl;
        conn->close();
        return true;
    }

    mysqlpp_bind *bind = conn->get_exec_bind();
    //bind->set_int(1, 3);

    conn->set_user_callback(&db_test_task::query_callback2_2); 

    conn->execute();

    return false;
}

void Test_exec(evutil_socket_t fd, short what, void *arg) {
    mysqlpp_pool *pp = (mysqlpp_pool *)arg;

    db_test_task *task = new db_test_task();

    std::string sql = "select * from z1 limit 2";

    mysqlpp_conn *conn = pp->get_connection();
    task->conn = conn;

    conn->set_user_callback(&db_test_task::query_callback2_1);  // 设置用户回调函数
    conn->set_user_argument(task); // 设置用户回调函数参数

    conn->prepare(sql);
}

int main(int argc, const char **argv) {
    std::string host = "127.0.0.1";
    std::string user = "root";
    std::string passwd = "123456";
    std::string dbname = "test";
    int port = 3306;

    struct event *ev;
    struct timeval one_seconds = {1,0};

    struct event_base *evbase_ = event_base_new();;

    mysqlpp_pool pp(evbase_, host, port, user, passwd, dbname);

    ev = event_new(evbase_, -1, 0, Test_exec, &pp);
    event_add(ev, &one_seconds);

    event_base_dispatch(evbase_);

    return 0;
}

