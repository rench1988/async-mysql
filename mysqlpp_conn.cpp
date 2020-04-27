/**
 * @author rench
 * @email finyren@163.com
 * @create date 2020-04-16 09:04:20
 * @modify date 2020-04-16 09:04:20
 * @desc [description]
 */

#include "mysqlpp_conn.h"
#include "mysqlpp_pool.h"
#include <event.h>
#include <string.h>

#define NEXT_IMMEDIATE(conn, new_st) do { conn->_status= new_st; goto again; } while (0)

#define STRLEN 256

static my_bool yes = true;

static int parseInt(const char *s) {
    char *e;
	int i = (int)strtol(s, &e, 10);

	return i;
}

static long long parseLLong(const char *s) {
    char *e;
	long long ll = strtoll(s, &e, 10);

	return ll;
}

static double parseDouble(const char *s) {
    char *e;
	double d = strtod(s, &e);

	return d;
}

static bool str_byte_equal(const char *a, const char *b) {
	if (a && b) {
        while (*a && *b)
            if (*a++ != *b++) return false;
        return (*a == *b);
    }
    return false;
}

mysqlpp_bind::mysqlpp_bind(int size) {
    _size = size;
    _bind = new MYSQL_BIND[_size];
    _params = new param_t[_size];
}

mysqlpp_bind::~mysqlpp_bind() {
    delete [] _bind;
    delete [] _params;
}

bool mysqlpp_bind::bind_stmt(MYSQL_STMT *stmt) {
    bool err = false;

    if (_size) {
        err = mysql_stmt_bind_param(stmt, _bind);
    }

    return err;
}

bool mysqlpp_bind::set_string(int parameterIndex, const char *x) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    _bind[i].buffer_type = MYSQL_TYPE_STRING;
    _bind[i].buffer = (char *)x;
    if (!x) {
        _params[i].length = 0;
        _bind[i].is_null = &yes;
    } else {
        _params[i].length = strlen(x);
        _bind[i].is_null = 0;
    }
    _bind[i].length = &_params[i].length;

    return true;
}

bool mysqlpp_bind::set_int(int parameterIndex, int x) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    _params[i].type.integer = x;
    _bind[i].buffer_type = MYSQL_TYPE_LONG;
    _bind[i].buffer = &_params[i].type.integer;
    _bind[i].is_null = 0;

    return true;
}

bool mysqlpp_bind::set_llong(int parameterIndex, long long x) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    _params[i].type.llong = x;
    _bind[i].buffer_type = MYSQL_TYPE_LONGLONG;
    _bind[i].buffer = &_params[i].type.llong;
    _bind[i].is_null = 0;  

    return true;
}

bool mysqlpp_bind::set_double(int parameterIndex, double x) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    _params[i].type.real = x;
    _bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
    _bind[i].buffer = &_params[i].type.real;
    _bind[i].is_null = 0;    

    return true;
}

bool mysqlpp_bind::set_timestamp(int parameterIndex, time_t x) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    struct tm ts;
    ts.tm_isdst = -1;

    gmtime_r(&x, &ts);
    _params[i].type.timestamp.year = ts.tm_year + 1900;
    _params[i].type.timestamp.month = ts.tm_mon + 1;
    _params[i].type.timestamp.day = ts.tm_mday;
    _params[i].type.timestamp.hour = ts.tm_hour;
    _params[i].type.timestamp.minute = ts.tm_min;
    _params[i].type.timestamp.second = ts.tm_sec;
    _bind[i].buffer_type = MYSQL_TYPE_TIMESTAMP;
    _bind[i].buffer = &_params[i].type.timestamp;
    _bind[i].is_null = 0;   

    return true;
}

bool mysqlpp_bind::set_blob(int parameterIndex, const void *x, int size) {
    int i = parameterIndex - 1;
    
    if (i < 0 || i >= _size) {
        return false;
    }

    _bind[i].buffer_type = MYSQL_TYPE_BLOB;
    _bind[i].buffer = (void*)x;
    if (!x) {
        _params[i].length = 0;
        _bind[i].is_null = &yes;
    } else {
        _params[i].length = size;
        _bind[i].is_null = 0;
    }
    _bind[i].length = &_params[i].length;

    return true;
}

mysqlpp_result::mysqlpp_result(int columnCount, MYSQL_RES *meta, MYSQL_STMT *stmt)
    : _columnCount(columnCount),
      _meta(meta),
      _stmt(stmt) {
    _needRebind = false;

    _bind = new MYSQL_BIND[columnCount];
    _columns = new column_t[columnCount];

    for (int i = 0; i < columnCount; i++) {
        _columns[i].buffer = new char[STRLEN + 1];
        _bind[i].buffer_type = MYSQL_TYPE_STRING;
        _bind[i].buffer = _columns[i].buffer;
        _bind[i].buffer_length = STRLEN;
        _bind[i].is_null = &_columns[i].is_null;
        _bind[i].length = &_columns[i].real_length;
        _columns[i].field = mysql_fetch_field_direct(meta, i);
    }
}

int mysqlpp_result::bind_stmt_result() {
    return mysql_stmt_bind_result(_stmt, _bind);
}

int mysqlpp_result::get_index(const char *name) {
    for (int i = 0; i < _columnCount; i++) {
        if (str_byte_equal(name, _columns[i].field->name)) {
            return i + 1;
        }
    }

    return -1;
}

void mysqlpp_result::_ensure_capacity(int index) {
    if (_columns[index].real_length <= _bind[index].buffer_length) 
        return;

    delete _columns[index].buffer;

    _columns[index].buffer = new char[_columns[index].real_length + 1];

    _bind[index].buffer = _columns[index].buffer;
    _bind[index].buffer_length = _columns[index].real_length;

    mysql_stmt_fetch_column(_stmt, &_bind[index], index, 0);

    _needRebind = true;   
}

const char *mysqlpp_result::get_string(int columnIndex) {
    int i = columnIndex - 1;

    if (i < 0 || i >= _columnCount) {
        return nullptr;
    }

    if (_columns[i].is_null)
        return nullptr;

    _ensure_capacity(i);
    _columns[i].buffer[_columns[i].real_length] = 0;

    return _columns[i].buffer;    
}

const char *mysqlpp_result::get_string_by_name(const char *columnName) {
    return get_string(get_index(columnName));
}

const void *mysqlpp_result::get_blob(int columnIndex, int &size) {
    int i = columnIndex - 1;

    if (i < 0 || i >= _columnCount) {
        return nullptr;
    }

    if (_columns[i].is_null)
        return nullptr;
        
    _ensure_capacity(i);
    size = (int)_columns[i].real_length;

    return _columns[i].buffer;    
}

const void *mysqlpp_result::get_blob_by_name(const char *columnName, int &size) {
    return get_blob(get_index(columnName), size);
}

int mysqlpp_result::get_int(int columnIndex, bool &is_null) {
    int i = columnIndex - 1;

    if (i < 0 || i >= _columnCount) {
        return 0;
    }

    if (_columns[i].is_null) {
        is_null = true;
        return 0;
    }

    const char *s = get_string(columnIndex);

    return s ? parseInt(s) : 0;
}

int mysqlpp_result::get_int_by_name(const char *columnName, bool &is_null) {
    return get_int(get_index(columnName), is_null);
}

long long mysqlpp_result::get_llong(int columnIndex, bool &is_null) {
    int i = columnIndex - 1;

    if (i < 0 || i >= _columnCount) {
        return 0;
    }

    if (_columns[i].is_null) {
        is_null = true;
        return 0;
    }

    const char *s = get_string(columnIndex);

    return s ? parseLLong(s) : 0;
}

long long mysqlpp_result::get_llong_by_name(const char *columnName, bool &is_null) {
    return get_llong(get_index(columnName), is_null);
}

double mysqlpp_result::get_double(int columnIndex, bool &is_null) {
    int i = columnIndex - 1;

    if (i < 0 || i >= _columnCount) {
        return 0;
    }

    if (_columns[i].is_null) {
        is_null = true;
        return 0;
    }

    const char *s = get_string(columnIndex);

    return s ? parseDouble(s) : 0;    
}

double mysqlpp_result::get_double_by_name(const char *columnName, bool &is_null) {
    return get_double(get_index(columnName), is_null);
}

mysqlpp_result::~mysqlpp_result() {
    for (int i = 0; i < _columnCount; i++) {
        delete [] _columns[i].buffer;
    }

    delete [] _bind;
    delete [] _columns;

    mysql_free_result(_meta);
}

mysqlpp_conn::mysqlpp_conn(struct event_base *loop,
                       const std::string &host,
                       const int port,
                       const std::string &user,
                       const std::string &passwd,
                       const std::string &dbname,
                       mysqlpp_pool *pp) 
    : _exec_result(nullptr),
      _user_callback(nullptr),
      _user_argument(nullptr),
      _failed(false),
      _eof(false),
      _closing(false),
      _pp(pp),
      _attached(false),
      _connected(false),
      _prepared(false),
      _available(false),
      _bind(nullptr),
      _state_machine(nullptr),
      _loop(loop),
      _columns(0),
      _exec_flag(false),
      _host(host),
      _user(user),
      _passwd(passwd),
      _dbname(dbname),
      _port(port),
      _err(0),
      _e(false),
      _status(CONNECT_START) {
    set_def_option();

    _event = new event;
    memset(_event, 0, sizeof(struct event));
}

void mysqlpp_conn::set_def_option() {
    mysql_init(&_mysql);
    mysql_options(&_mysql, MYSQL_READ_DEFAULT_GROUP, "mysql++");  // mysql++ for option file
    mysql_options(&_mysql, MYSQL_OPT_NONBLOCK, 0);  // 0 for stack size, 0 is default value
}


// 析构之前已经调用了close, close里面会cleanup
mysqlpp_conn::~mysqlpp_conn() {
    delete _event;

    mysql_close(&_mysql);
}

void mysqlpp_conn::init_library(int argc, const char **argv, char **groups) {
    mysql_library_init(argc, (char **)argv, groups);
}

void mysqlpp_conn::detach_event() {
    if (_attached) {
        event_del(_event);;
    }

    _attached = false;

    memset(_event, 0, sizeof(struct event));
}

void mysqlpp_conn::free_result() {
    if (_result) {
        mysql_free_result(_result); // must be called after fetch all result, otherwise will blocking
    }

    _result = nullptr;
    _eof = false;
}

void mysqlpp_conn::free_stmt_blocking() {
    if (_stmt) {
        mysql_stmt_close(_stmt);
    }

    _stmt = nullptr;
}

void mysqlpp_conn::unset_callback() {
    _user_callback = nullptr;
    _user_argument = nullptr;
}

void mysqlpp_conn::cleanup() {
    if (_exec_result) {
        delete _exec_result;
        _exec_result = nullptr;
    }

    _failed = false;
    _eof = false;
    _closing = false;

    _sb.clear();

    detach_event();

    // _stmt shoule be close in event loop, because mysql_stmt_close will blocking

    free_result();
    free_stmt_blocking();

    if (_bind) {
        delete _bind;
        _bind = nullptr;
    }

    _state_machine = nullptr;
    _columns = 0;
    _exec_flag = false;
    _err = 0;
    _e = false;

    _sql.clear();

    _status = CONNECT_START;
}

void mysqlpp_conn::next_event(Estatus new_status, int status) {
    short wait_event= 0;
    struct timeval tv, *ptv;
    int fd;

    detach_event();  // first detach event

    if (status & MYSQL_WAIT_READ)
        wait_event |= EV_READ;
    if (status & MYSQL_WAIT_WRITE)
        wait_event |= EV_WRITE;
    if (wait_event)
        fd= mysql_get_socket(&_mysql);
    else
        fd= -1;

    if (status & MYSQL_WAIT_TIMEOUT) {
        tv.tv_sec= mysql_get_timeout_value(&_mysql);
        tv.tv_usec= 0;
        ptv= &tv;
    } else {
        ptv= NULL;
    }

    ::event_set(_event, fd, wait_event, this->_state_machine, this);
    ::event_base_set(_loop, _event);
    ::event_add(_event, ptv);

    _attached = true;

    _status = new_status;
}

int mysqlpp_conn::mysql_status(short event) {
    int status= 0;
    if (event & EV_READ)
        status|= MYSQL_WAIT_READ;
    if (event & EV_WRITE)
        status|= MYSQL_WAIT_WRITE;
    if (event & EV_TIMEOUT)
        status|= MYSQL_WAIT_TIMEOUT;
    return status;
}

const char *mysqlpp_conn::error() {
    if (!_sb.empty()) {
        return _sb.c_str();
    }

    return mysql_error(&_mysql);
}

void mysqlpp_conn::conn_done() {
    if (!_ret) {
        _failed = true;
        _user_callback(this, _user_argument);  // user close and destroy it!
        return;
    }

    _connected = true;

    if (_exec_flag) {
        _status = PREPARE_START;
        _state_machine = &prepare_state_machine;
    } else {
        _status = QUERY_START;
        _state_machine = &query_state_machine;
    }

    _state_machine(-1, -1, this);
}

void mysqlpp_conn::query_done() {
    if (mysql_errno(&_mysql)) {
        _failed = true;
        _user_callback(this, _user_argument);
        return;
    }

    if (!_result) {
        _user_callback(this, _user_argument);
        return;
    }

    _columns = mysql_field_count(&_mysql);

    _status = FETCH_ROW_START;
    _state_machine = &fetch_state_machine;
    _state_machine(-1, -1, this);
}

int mysqlpp_conn::row_done() {
    int ret = 0;

    if (mysql_errno(&_mysql)) {
        _failed = true;
        ret = 1;
    }

    if (ret == 0 && _row == nullptr) {
        _eof = true;
        ret = 1;
    }

    bool done = _user_callback(this, this->_user_argument);
    if (done || _closing) {
        ret = 1;
    }

    return ret;
}

void mysqlpp_conn::prepare_done() {
    if (_err) {
        _failed = true;
        _user_callback(this, _user_argument);
        return;
    }

    _prepared = true;

    int size = mysql_stmt_param_count(_stmt);
    if (size) {
        _bind = new mysqlpp_bind(size);
    }

    _user_callback(this, _user_argument);  // bind input argument in this calling. user calling execute
    return;
}

void mysqlpp_conn::close_stmt_done() {
    _stmt = nullptr;

    if (_user_callback) {  // user calling close stmt or stmt error
        cleanup();
        _user_callback(this, _user_argument);
        return;
    }

    add_to_connection_pool();

    return;
}

void mysqlpp_conn::close_done() {
    _closing = false;
    add_to_connection_pool();
}

void mysqlpp_conn::execute_done() {
    int columns;
    MYSQL_RES *meta;
    
    if (_err) {
        goto failed;
    }

    columns = mysql_stmt_field_count(_stmt);
    if (!columns) {
        _user_callback(this, _user_argument);
        return;
    }

    meta = mysql_stmt_result_metadata(_stmt);
    if (!meta)
        goto failed;

    _exec_result = new mysqlpp_result(columns, meta, _stmt);

    _err = _exec_result->bind_stmt_result();
    if (_err)
        goto failed;

    _status = STMT_FETCH_START;
    _state_machine = &stmt_fetch_state_machine;
    _state_machine(-1, -1, this);    

    return;

failed:
    _failed = true;
    _user_callback(this, _user_argument);
    return;
}

bool mysqlpp_conn::stmt_fetch_done() {
    if (_err == 1) {
        _failed = true;
    }

    if (_err == MYSQL_NO_DATA) {
        _eof = true;
    }

    return _user_callback(this, this->_user_argument);
}

uint64_t mysqlpp_conn::affected_rows() {
    return mysql_affected_rows(&_mysql);
}

uint64_t mysqlpp_conn::insert_id() {
    return mysql_insert_id(&_mysql);
}

void mysqlpp_conn::stmt_fetch_state_machine(int sockfd, short event, void *v) {
    int  status;
    bool done;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case STMT_FETCH_START:
        status = mysql_stmt_fetch_start(&conn->_err, conn->_stmt);
        if (status)
            conn->next_event(STMT_FETCH_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, STMT_FETCH_DONE);
        break;
    case STMT_FETCH_WAITING:
        status = mysql_stmt_fetch_cont(&conn->_err, conn->_stmt, mysql_status(event));
        if (status)
            conn->next_event(STMT_FETCH_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, STMT_FETCH_DONE);
        break;
    case STMT_FETCH_DONE:
        done = conn->stmt_fetch_done();
        if (done || conn->_closing) 
            break;
        else 
            NEXT_IMMEDIATE(conn, STMT_FETCH_START);
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::conn_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case CONNECT_START:
        status = mysql_real_connect_start(&conn->_ret, &conn->_mysql, conn->_host.c_str(), conn->_user.c_str(), 
            conn->_passwd.c_str(), conn->_dbname.c_str(), conn->_port, NULL, 0);
        if (status)
            conn->next_event(CONNECT_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, CONNECT_DONE);
        break;

    case CONNECT_WAITING:
        status = mysql_real_connect_cont(&conn->_ret, &conn->_mysql, mysql_status(event));
        if (status)
            conn->next_event(CONNECT_WAITING, status);
        else
            NEXT_IMMEDIATE(conn, CONNECT_DONE);
        break;

    case CONNECT_DONE:
        conn->conn_done();  // set state machine function pointer and conn->_status
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::close_stmt_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case CLOSE_STMT_START:
        status = mysql_stmt_close_start(&conn->_e, conn->_stmt);
        if (status)
            conn->next_event(CLOSE_STMT_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, CLOSE_STMT_DONE);
        break;
    
    case CLOSE_STMT_WAITING:
        status = mysql_stmt_close_cont(&conn->_e, conn->_stmt, mysql_status(event));
        if (status)
            conn->next_event(CLOSE_STMT_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, CLOSE_STMT_DONE);
        break;
    
    case CLOSE_STMT_DONE:
        conn->close_stmt_done();
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::prepare_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

    if (!conn->_stmt)
        conn->_stmt = mysql_stmt_init(&conn->_mysql); // init mysql prepare statement

again:
    switch (conn->_status) {
    case PREPARE_START:
        status = mysql_stmt_prepare_start(&conn->_err, conn->_stmt, conn->_sql.c_str(), conn->_sql.size());
        if (status)
            conn->next_event(PREPARE_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, PREPARE_DONE);
        break;

    case PREPARE_WAITING:
        status = mysql_stmt_prepare_cont(&conn->_err, conn->_stmt, mysql_status(event));
        if (status)
            conn->next_event(PREPARE_WAITING, status);
        else
            NEXT_IMMEDIATE(conn, PREPARE_DONE);
        break;

    case PREPARE_DONE:
        conn->prepare_done();
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::query_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case QUERY_START:
        status = mysql_real_query_start(&conn->_err, &conn->_mysql, conn->_sql.c_str(), conn->_sql.size());
        if (status)
            conn->next_event(QUERY_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, QUERY_RESULT_READY);
        break;

    case QUERY_WAITING:
        status = mysql_real_query_cont(&conn->_err, &conn->_mysql, mysql_status(event));
        if (status)
            conn->next_event(QUERY_WAITING, status);
        else
            NEXT_IMMEDIATE(conn, QUERY_RESULT_READY);
        break;
    case QUERY_RESULT_READY:
        conn->_result = mysql_use_result(&conn->_mysql);

        conn->query_done();
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::fetch_state_machine(int sockfd, short event, void *v) {
    int ret;
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case FETCH_ROW_START:
        status = mysql_fetch_row_start(&conn->_row, conn->_result);
        if (status)
            conn->next_event(FETCH_ROW_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, FETCH_ROW_RESULT_READY);
        break;
    case FETCH_ROW_WAITING:
        status = mysql_fetch_row_cont(&conn->_row, conn->_result, mysql_status(event));
        if (status)
            conn->next_event(FETCH_ROW_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, FETCH_ROW_RESULT_READY);
        break;
    case FETCH_ROW_RESULT_READY:
        ret = conn->row_done();
        if (ret)
            return;  // all rows being readed or error happened.
        else 
            NEXT_IMMEDIATE(conn, FETCH_ROW_START);     
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::close_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case CLOSE_START:
        status = mysql_close_start(&conn->_mysql);
        if (status)
            conn->next_event(CLOSE_WAITING, status);
        else
            NEXT_IMMEDIATE(conn, CLOSE_DONE);
        break;
    case CLOSE_WAITING:
        status = mysql_close_cont(&conn->_mysql, mysql_status(event));
        if (status)
            conn->next_event(CLOSE_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, CLOSE_DONE);
        break;
    case CLOSE_DONE:
        conn->close_done();
        break;
    
    default:
        break;
    }

    return;
}

void mysqlpp_conn::execute_state_machine(int sockfd, short event, void *v) {
    int status;
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

again:
    switch (conn->_status) {
    case EXECUTE_START:
        status = mysql_stmt_execute_start(&conn->_err, conn->_stmt);
        if (status)
            conn->next_event(EXECUTE_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, EXECUTE_DONE);
        break;
    case EXECUTE_WAITING:
        status = mysql_stmt_execute_cont(&conn->_err, conn->_stmt, mysql_status(event));
        if (status)
            conn->next_event(EXECUTE_WAITING, status);
        else 
            NEXT_IMMEDIATE(conn, EXECUTE_DONE);
        break;
    case EXECUTE_DONE:
        conn->execute_done();
        break;
    default:
        break;
    }

    return;
}

void mysqlpp_conn::connect() {
    _status = CONNECT_START;
    _state_machine = &conn_state_machine;

    _state_machine(-1, -1, this);
}

void mysqlpp_conn::add_to_connection_pool() {
    cleanup();
    unset_callback();

    _pp->add_connection(this);
}


void mysqlpp_conn::close_callback(int sockfd, short event, void *v) {
    mysqlpp_conn *conn = (mysqlpp_conn *)v;

    conn->add_to_connection_pool();
    return;
}

void mysqlpp_conn::close() {
    _user_callback = nullptr;
    _user_argument = nullptr;

    _closing = true;

    detach_event();

    event_base_once(_loop, -1, EV_TIMEOUT, close_callback, this, NULL);
}

// interface for user calling

void mysqlpp_conn::query(std::string &sql) {
    cleanup();

    _sql = sql;
    _exec_flag = false;

    if (!_connected) {
        connect();
        return;
    }

    _status = QUERY_START;
    _state_machine = &query_state_machine;

    _state_machine(-1, -1, this);
}

void mysqlpp_conn::prepare(std::string &sql) {
    cleanup();

    _sql = sql;
    _exec_flag = true;

    if (!_connected) {
        connect();
        return;
    }

    _status = PREPARE_START;
    _state_machine = &prepare_state_machine;

    _state_machine(-1, -1, this);
}

void mysqlpp_conn::execute() {
    if (!_connected || !_prepared) {
        _failed = true;
        _sb = "execute should prepared first";
        _user_callback(this, _user_argument);
        return;
    }

    if (_bind && _bind->bind_stmt(_stmt)) {
        _failed = true;
        _user_callback(this, _user_argument);
        return;
    }

    _status = EXECUTE_START;
    _state_machine = &execute_state_machine;

    _state_machine(-1, -1, this);
}

void mysqlpp_conn::execute_query() {
    execute();
}
