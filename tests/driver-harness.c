/* DB-free mocks, not vendor declarations. The tested functions are extracted
 * from odbc_driver.c; quoting comes from main/php_odbc_utils.c unchanged. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "odbc_zos_driver.h"

#define PDO_ODBC_DB2_ZOS 1
#define PHPAPI
#define SQL_SUCCESS 0
#define SQL_SUCCESS_WITH_INFO 1
#define SQL_ERROR (-1)
#define SQL_NO_DATA 100
#define SQL_NULL_HANDLE 0
#define SQL_NULL_HDBC 0
#define SQL_NULL_HENV 0
#define SQL_HANDLE_ENV 1
#define SQL_HANDLE_DBC 2
#define SQL_HANDLE_STMT 3
#define SQL_ATTR_ODBC_VERSION 4
#define SQL_OV_ODBC3 3
#define SQL_ATTR_AUTOCOMMIT 5
#define SQL_AUTOCOMMIT_ON 1
#define SQL_AUTOCOMMIT_OFF 0
#define SQL_IS_INTEGER (-6)
#define SQL_COMMIT 0
#define SQL_ROLLBACK 1
#define SQL_ODBC_CURSORS 110
#define SQL_CUR_USE_IF_NEEDED 0
#define SQL_CUR_USE_ODBC 1
#define SQL_CUR_USE_DRIVER 2
#define SQL_DRIVER_NOPROMPT 0
#define SQL_NTS (-3)
#define PDO_ODBC_ATTR_ASSUME_UTF8 1000
#define PDO_ODBC_ATTR_USE_CURSOR_LIBRARY 1001
#define PDO_ATTR_AUTOCOMMIT 0

typedef int RETCODE;
typedef int32_t SQLINTEGER;
typedef int32_t SQLLEN;
typedef int16_t SQLSMALLINT;
typedef uint16_t SQLUSMALLINT;
typedef unsigned char SQLCHAR;
typedef void *SQLPOINTER;
typedef int PDO_ODBC_HSTMT;
typedef long zend_long;
typedef unsigned long zend_ulong;
typedef struct { const char *text; size_t length; } zend_string;
#define ZSTR_VAL(s) ((s)->text)
#define ZSTR_LEN(s) ((s)->length)
typedef struct {
    bool boolean;
    char *text;
    size_t length;
    bool has_cursor;
    zend_long cursor_lib;
} zval;
typedef struct {
    char last_err_msg[256], last_state[6];
    const char *what;
} mock_error;
typedef struct {
    int env, dbc;
    bool assume_utf8;
    mock_error einfo;
} pdo_odbc_db_handle;
enum pdo_error_mode { PDO_ERRMODE_SILENT, PDO_ERRMODE_WARNING, PDO_ERRMODE_EXCEPTION };
typedef struct {
    void *driver_data;
    enum pdo_error_mode error_mode;
    bool is_persistent, auto_commit, in_txn;
    char error_code[6];
    const char *data_source;
    char *username, *password;
    size_t data_source_len;
    const void *methods;
    int alloc_own_columns;
} pdo_dbh_t;
static const int odbc_methods = 0;
static int calls, errors, set_rc, info_reported, alloc_rc;
static int alloc_calls, manager_attr_calls;
static const char *last_message;
static pdo_dbh_t *observed_dbh;
static bool observed_autocommit;
static char connected_string[INT16_MAX + 1];
static void *checked_alloc(size_t n) { void *p = malloc(n); assert(p); return p; }
#define emalloc(n) checked_alloc(n)
#define efree(p) free(p)
#define pemalloc(n, persistent) checked_alloc(n)
#define pecalloc(n, size, persistent) calloc(n, size)
#define pefree(p, persistent) free(p)
static void pdo_raise_impl_error(pdo_dbh_t *dbh, void *stmt, const char *state, const char *message)
{
    (void)stmt; last_message = message; errors++; memcpy(dbh->error_code, state, 6);
    if (!strcmp(state, "IM001")) assert(dbh->error_mode == PDO_ERRMODE_EXCEPTION);
}
#define pdo_odbc_drv_error(what) pdo_raise_impl_error(dbh, NULL, "HY000", what)
#define pdo_odbc_doer_error(what) pdo_odbc_drv_error(what)
static bool pdo_get_bool_param(bool *out, zval *value) { *out = value->boolean; return true; }
static zend_ulong pdo_attr_lval(zval *options, int attr, zend_ulong fallback)
{
    return options && attr == PDO_ODBC_ATTR_USE_CURSOR_LIBRARY && options->has_cursor
        ? (zend_ulong)options->cursor_lib : fallback;
}
#define Z_ARRVAL_P(value) (value)
#define ZVAL_DEREF(value) ((void)(value))
/* Mock options: the boolean UTF8 flag plus an optional cursor-library value. */
static zval *zend_hash_index_find(zval *options, zend_ulong attr)
{ return attr == PDO_ODBC_ATTR_ASSUME_UTF8 ? options : NULL; }
static int SQLAllocHandle(int type, int parent, int *handle)
{ (void)type; (void)parent; calls++; alloc_calls++; *handle = alloc_rc == SQL_ERROR ? 0 : 7; return alloc_rc; }
static int SQLFreeHandle(int type, int handle)
{ (void)type; assert(handle); calls++; return SQL_SUCCESS; }
static int SQLDisconnect(int handle) { assert(handle); calls++; return SQL_SUCCESS; }
static int SQLEndTran(int type, int handle, int operation)
{ (void)type; (void)operation; assert(handle); calls++; return set_rc; }
static int SQLSetEnvAttr(int handle, int attr, void *value, int length)
{ (void)attr; (void)value; (void)length; assert(handle); calls++; return set_rc; }
static int SQLSetConnectAttr(int handle, int attr, void *value, int length)
{
    (void)value; (void)length; assert(handle); calls++;
    if (attr == SQL_ODBC_CURSORS) { manager_attr_calls++; return SQL_ERROR; }
    if (observed_dbh) observed_autocommit = observed_dbh->auto_commit;
    return set_rc;
}
static int SQLDriverConnect(int handle, void *window, SQLCHAR *input, SQLSMALLINT input_len,
    SQLCHAR *output, SQLSMALLINT capacity, SQLSMALLINT *output_len, int completion)
{
    (void)window; (void)output; (void)capacity; (void)completion;
    assert(handle && input_len >= 0 && strlen((char *)input) == (size_t)input_len);
    memcpy(connected_string, input, (size_t)input_len + 1); *output_len = 0; calls++;
    return SQL_SUCCESS;
}
static int SQLConnect(int handle, SQLCHAR *dsn, SQLSMALLINT dl, SQLCHAR *uid,
    SQLSMALLINT ul, SQLCHAR *pwd, SQLSMALLINT pl)
{
    (void)dsn; (void)uid; (void)pwd; assert(handle && dl == SQL_NTS && ul == SQL_NTS && pl == SQL_NTS);
    calls++; return SQL_SUCCESS;
}
static int SQLExecDirect(int handle, SQLCHAR *sql, SQLINTEGER length)
{ (void)sql; assert(handle && length >= 0); calls++; return SQL_SUCCESS; }
static int SQLRowCount(int handle, SQLLEN *count)
{ assert(handle); calls++; *count = 42; return SQL_SUCCESS; }
static int SQLGetInfo(int handle, SQLUSMALLINT type, void *buffer, SQLSMALLINT capacity, SQLSMALLINT *length)
{
    (void)type; assert(handle); calls++; memset(buffer, 'x', (size_t)capacity);
    *length = (SQLSMALLINT)info_reported; return SQL_SUCCESS_WITH_INFO;
}
static void mock_string(zval *value, const char *text, size_t length)
{
    value->text = checked_alloc(length + 1); memcpy(value->text, text, length);
    value->text[length] = 0; value->length = length;
}
#define ZVAL_STRINGL(value, text, length) mock_string(value, text, length)
#include "driver-under-test.h"

static char *copy(const char *s)
{ size_t n = strlen(s) + 1; char *p = checked_alloc(n); memcpy(p, s, n); return p; }
static void credentials(const char *dsn, unsigned expected)
{ assert(pdo_odbc_zos_credentials(dsn) == expected); }
static void completion(const char *dsn, char *uid, char *pwd, const char *expected)
{
    pdo_dbh_t dbh = {0}; dbh.data_source = copy(dsn); dbh.username = uid; dbh.password = pwd;
    assert(pdo_odbc_zos_complete_connstr(&dbh)); assert(!strcmp(dbh.data_source, expected));
    free((void *)dbh.data_source);
}
int main(void)
{
    credentials("", 0);
    credentials("DSN=uid=inside;X=PWD=inside", 0);
    credentials("DSN={name;UID=fake;PWD=fake}", 0);
    credentials("DSN= {a}};UID=fake;PWD=fake};pWd=", PDO_ODBC_ZOS_PWD);
    credentials("X={unclosed;UID=fake;PWD=fake", 0);
    credentials(" ; \tUiD \t=; pWd ={};", PDO_ODBC_ZOS_UID | PDO_ODBC_ZOS_PWD);
    credentials("XUID=x;XPWD=x;UIDsuffix=x;PWDsuffix=x", 0);
    credentials("UID={a}}};DSN={x;PWD=fake}", PDO_ODBC_ZOS_UID);
    completion("DSN=x", "", "", "DSN=x;UID=;PWD=");
    completion("DSN=x;uId=dsn", "arg", "secret", "DSN=x;uId=dsn;PWD=secret");
    completion("DSN=x;PwD=dsn", "arg", "secret", "DSN=x;PwD=dsn;UID=arg");
    completion("DSN=x;UID=;PWD=", "arg", "secret", "DSN=x;UID=;PWD=");
    completion("DSN={x;UID=fake;PWD=fake}", "a;b", "p}q", "DSN={x;UID=fake;PWD=fake};UID={a;b};PWD={p}}q}");
    completion("DSN=x", NULL, "", "DSN=x;PWD=");
    completion("DSN=x", "", NULL, "DSN=x;UID=");
    completion("DSN=x", NULL, NULL, "DSN=x");
    completion("DSN=x", "{already;quoted}", NULL, "DSN=x;UID={already;quoted}");

    assert(pdo_odbc_zos_sql_length_valid(INT32_MAX));
    assert(!pdo_odbc_zos_sql_length_valid((size_t)INT32_MAX + 1));
    assert(pdo_odbc_zos_connstr_length_valid(INT16_MAX));
    assert(!pdo_odbc_zos_connstr_length_valid((size_t)INT16_MAX + 1));
    size_t n = INT16_MAX - 5;
    assert(pdo_odbc_zos_append_length(&n, 0) && n == INT16_MAX);
    assert(!pdo_odbc_zos_append_length(&n, 0) && n == INT16_MAX);
    n = SIZE_MAX; assert(!pdo_odbc_zos_append_length(&n, 0));
    n = 0; assert(!pdo_odbc_zos_append_length(&n, SIZE_MAX));
    assert(pdo_odbc_zos_prepare_text_valid("SELECT 1", 8));
    assert(!pdo_odbc_zos_prepare_text_valid("a\0b", 3));
    assert(!pdo_odbc_zos_prepare_text_valid("", (size_t)INT32_MAX + 1));
    assert(pdo_odbc_zos_output_length(32767, 256) == 255);
    assert(pdo_odbc_zos_output_length(256, 256) == 255);
    assert(pdo_odbc_zos_output_length(12, 256) == 12);
    assert(pdo_odbc_zos_output_length(-1, 256) == 0);
    assert(pdo_odbc_zos_output_length(2, 0) == 0);

    /* NULL DSN is a sentinel: persistence must reject before even inspecting
     * it, allocating a native handle, or calling any mock SQL function. */
    pdo_dbh_t dbh = {0}; dbh.is_persistent = true;
    assert(!pdo_odbc_handle_factory(&dbh, NULL));
    assert(!strcmp(dbh.error_code, "IM001") && calls == 0);
    assert(dbh.driver_data == NULL && dbh.methods == NULL);
    assert(dbh.error_mode == PDO_ERRMODE_SILENT);
    dbh.error_mode = PDO_ERRMODE_WARNING;
    assert(!pdo_odbc_handle_factory(&dbh, NULL));
    assert(dbh.error_mode == PDO_ERRMODE_WARNING && calls == 0);
    odbc_handle_closer(&dbh); assert(calls == 0);

    zval utf8_option = {0}; utf8_option.boolean = true;
    for (int mode = PDO_ERRMODE_SILENT; mode <= PDO_ERRMODE_EXCEPTION; mode++) {
        dbh = (pdo_dbh_t){0}; dbh.error_mode = (enum pdo_error_mode)mode;
        /* NULL DSN sentinel again: fail before runtime/DSN or SQL access. */
        assert(!pdo_odbc_handle_factory(&dbh, &utf8_option));
        assert(!strcmp(dbh.error_code, "IM001") && calls == 0);
        assert(dbh.driver_data == NULL && dbh.methods == NULL);
        assert(dbh.error_mode == (enum pdo_error_mode)mode);
    }
    utf8_option.boolean = false;
    dbh = (pdo_dbh_t){0}; dbh.data_source = copy("DSN=x;UID=dsn"); dbh.password = "";
    set_rc = SQL_SUCCESS_WITH_INFO;
    assert(pdo_odbc_handle_factory(&dbh, &utf8_option));
    assert(!((pdo_odbc_db_handle *)dbh.driver_data)->assume_utf8);
    assert(!strcmp(connected_string, "DSN=x;UID=dsn;PWD="));
    odbc_handle_closer(&dbh); free((void *)dbh.data_source);
    assert(dbh.driver_data == NULL);

    /* Default and explicit native modes connect without any manager attribute.
     * Exercise both SQLConnect and SQLDriverConnect factory branches. */
    const zend_long supported[] = {SQL_CUR_USE_IF_NEEDED, SQL_CUR_USE_DRIVER};
    for (int direct = 0; direct < 2; direct++) {
        for (int choice = -1; choice < 2; choice++) {
            zval option = {0}; option.has_cursor = true;
            if (choice >= 0) option.cursor_lib = supported[choice];
            dbh = (pdo_dbh_t){0}; dbh.data_source = copy(direct ? "DSN=x" : "x");
            int allocations_before = alloc_calls;
            assert(pdo_odbc_handle_factory(&dbh, choice < 0 ? NULL : &option));
            assert(alloc_calls == allocations_before + 2 && manager_attr_calls == 0);
            odbc_handle_closer(&dbh); free((void *)dbh.data_source);
        }
    }
    const zend_long rejected[] = {SQL_CUR_USE_ODBC, -1, 3, 42};
    for (size_t choice = 0; choice < sizeof(rejected) / sizeof(rejected[0]); choice++) {
        for (int mode = PDO_ERRMODE_SILENT; mode <= PDO_ERRMODE_EXCEPTION; mode++) {
            zval option = {0}; option.has_cursor = true; option.cursor_lib = rejected[choice];
            dbh = (pdo_dbh_t){0}; dbh.error_mode = (enum pdo_error_mode)mode;
            int calls_before = calls, allocations_before = alloc_calls, errors_before = errors;
            /* NULL DSN: reject before inspecting it or making ANY SQL call.
             * The error mock requires exception mode for IM001. */
            assert(!pdo_odbc_handle_factory(&dbh, &option));
            assert(!strcmp(dbh.error_code, "IM001") && errors == errors_before + 1);
            assert(strstr(last_message, "cursor library") && strstr(last_message, "not supported"));
            assert(calls == calls_before && alloc_calls == allocations_before);
            assert(dbh.driver_data == NULL && dbh.methods == NULL);
            assert(dbh.error_mode == (enum pdo_error_mode)mode);
            odbc_handle_closer(&dbh); assert(calls == calls_before);
        }
    }

    char *large = checked_alloc((size_t)INT16_MAX + 2);
    memset(large, 'x', (size_t)INT16_MAX + 1); large[0] = '='; large[INT16_MAX + 1] = 0;
    dbh = (pdo_dbh_t){0}; dbh.data_source = large; int before = calls;
    assert(!pdo_odbc_handle_factory(&dbh, NULL));
    assert(calls == before && dbh.driver_data == NULL && dbh.methods == NULL);
    large[INT16_MAX] = 0;
    assert(pdo_odbc_handle_factory(&dbh, NULL)); odbc_handle_closer(&dbh);
    dbh = (pdo_dbh_t){0}; dbh.data_source = large; dbh.username = ""; before = calls;
    assert(!pdo_odbc_handle_factory(&dbh, NULL)); assert(calls == before);
    free(large);

    /* Factory allocation failure still permits PDO cleanup, with no attempt
     * to free a null environment or a connection that was never allocated. */
    dbh = (pdo_dbh_t){0}; dbh.data_source = copy("DSN=x"); alloc_rc = SQL_ERROR;
    assert(!pdo_odbc_handle_factory(&dbh, NULL)); before = calls;
    odbc_handle_closer(&dbh); assert(calls == before); free((void *)dbh.data_source);
    alloc_rc = SQL_SUCCESS;

    pdo_odbc_db_handle handle = {0}; handle.dbc = 7;
    dbh = (pdo_dbh_t){0}; dbh.driver_data = &handle; dbh.auto_commit = true;
    zval value = {0}; dbh.error_mode = PDO_ERRMODE_EXCEPTION;
    int utf8_before = calls;
    value.boolean = true;
    assert(!odbc_handle_set_attr(&dbh, PDO_ODBC_ATTR_ASSUME_UTF8, &value));
    assert(!strcmp(dbh.error_code, "IM001") && !handle.assume_utf8 && calls == utf8_before);
    value.boolean = false;
    assert(odbc_handle_set_attr(&dbh, PDO_ODBC_ATTR_ASSUME_UTF8, &value));
    assert(!handle.assume_utf8 && calls == utf8_before);
    observed_dbh = &dbh; set_rc = SQL_ERROR;
    assert(!odbc_handle_set_attr(&dbh, PDO_ATTR_AUTOCOMMIT, &value));
    assert(dbh.auto_commit && observed_autocommit);
    set_rc = SQL_SUCCESS_WITH_INFO;
    assert(odbc_handle_set_attr(&dbh, PDO_ATTR_AUTOCOMMIT, &value));
    assert(!dbh.auto_commit && observed_autocommit);
    value.boolean = true;
    assert(odbc_handle_set_attr(&dbh, PDO_ATTR_AUTOCOMMIT, &value));
    assert(dbh.auto_commit && !observed_autocommit);
    before = errors;
    assert(odbc_handle_begin(&dbh) && odbc_handle_commit(&dbh) && odbc_handle_rollback(&dbh));
    assert(errors == before); observed_dbh = NULL;

    zend_string sql = {"", (size_t)INT32_MAX + 1}; before = calls;
    assert(odbc_handle_doer(&dbh, &sql) == -1 && calls == before);
    sql.length = INT32_MAX; assert(odbc_handle_doer(&dbh, &sql) == 42);
    info_reported = INT16_MAX; assert(pdo_odbc_get_info_string(&dbh, 0, &value) == 1);
    assert(value.length == 255 && value.text[254] == 'x'); free(value.text);
    info_reported = -1; assert(pdo_odbc_get_info_string(&dbh, 0, &value) == -1);
    assert(manager_attr_calls == 0);
    puts("native driver DB-free tests: PASS");
    return 0;
}
