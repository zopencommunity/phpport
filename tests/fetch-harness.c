/* Compile the actual driver fetch function against a small mock ODBC API.
 * This checks dispatch/bounds semantics, not Db2 connectivity or LOB data. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PDO_ODBC_DB2_ZOS 1
#define SQL_SUCCESS 0
#define SQL_SUCCESS_WITH_INFO 1
#define SQL_NO_DATA 100
#define SQL_FETCH_NEXT 1
#define SQL_FETCH_PRIOR 2
#define SQL_FETCH_FIRST 3
#define SQL_FETCH_LAST 4
#define SQL_FETCH_ABSOLUTE 5
#define SQL_FETCH_RELATIVE 6

typedef int RETCODE;
typedef int16_t SQLSMALLINT;
typedef int32_t SQLLEN;
typedef int64_t zend_long;
enum pdo_fetch_orientation {
    PDO_FETCH_ORI_NEXT, PDO_FETCH_ORI_PRIOR, PDO_FETCH_ORI_FIRST,
    PDO_FETCH_ORI_LAST, PDO_FETCH_ORI_ABS, PDO_FETCH_ORI_REL
};
typedef struct { int stmt; } pdo_odbc_stmt;
typedef struct { void *driver_data; char error_code[6]; } pdo_stmt_t;

static int next_calls, scroll_calls, error_calls, result_code;
static SQLLEN last_offset;
static const char *last_api;
static RETCODE SQLFetch(int handle)
{
    assert(handle == 39);
    next_calls++;
    return result_code;
}
static RETCODE SQLFetchScroll(int handle, SQLSMALLINT orientation, SQLLEN offset)
{
    assert(handle == 39);
    assert(orientation != SQL_FETCH_NEXT);
    scroll_calls++;
    last_offset = offset;
    return result_code;
}
#define pdo_odbc_stmt_error(api) (error_calls++, last_api = (api))

#include "fetch-under-test.h"

static void reset(void)
{
    next_calls = scroll_calls = error_calls = 0;
    result_code = SQL_SUCCESS;
    last_offset = 123;
    last_api = "";
}
int main(void)
{
    pdo_odbc_stmt driver = {39};
    pdo_stmt_t stmt = {&driver, "00000"};
    enum pdo_fetch_orientation ignored[] = {
        PDO_FETCH_ORI_PRIOR, PDO_FETCH_ORI_FIRST, PDO_FETCH_ORI_LAST
    };
    unsigned i;
    reset();
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_NEXT, INT64_MAX) == 1);
    assert(next_calls == 1 && scroll_calls == 0);
    for (i = 0; i < sizeof(ignored) / sizeof(ignored[0]); i++) {
        reset();
        assert(odbc_stmt_fetch(&stmt, ignored[i], INT64_MAX) == 1);
        assert(scroll_calls == 1 && last_offset == 0);
    }
    reset();
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_ABS, INT64_MAX) == 0);
    assert(strcmp(stmt.error_code, "HY106") == 0 && scroll_calls == 0);
    reset();
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_REL, INT64_MIN) == 0);
    assert(scroll_calls == 0);
    reset();
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_ABS, INT32_MAX) == 1);
    assert(last_offset == INT32_MAX);
    reset();
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_REL, INT32_MIN) == 1);
    assert(last_offset == INT32_MIN);
    reset();
    result_code = SQL_SUCCESS_WITH_INFO;
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_NEXT, 0) == 1);
    assert(error_calls == 1 && strcmp(last_api, "SQLFetch") == 0);
    reset();
    result_code = SQL_NO_DATA;
    assert(odbc_stmt_fetch(&stmt, PDO_FETCH_ORI_NEXT, 0) == 0);
    assert(error_calls == 0);
    puts("Native fetch dispatch/bounds checks passed");
    return 0;
}
