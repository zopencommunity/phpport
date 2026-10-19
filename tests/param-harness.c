/* Standalone mocks, NOT IBM SDK declarations. Exercise the very same pure
 * helpers included by odbc_stmt.c with a 32-bit SQLLEN on an LP64 host. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t SQLLEN;
typedef int16_t SQLSMALLINT;
enum {
    SQL_NULL_DATA = -1, SQL_NO_TOTAL = -4,
    SQL_BINARY = 10, SQL_VARBINARY = 11, SQL_LONGVARBINARY = 12,
    SQL_BLOB = 13
};
/* SQL_BLOB is intentionally distinct from the standard binary types. */
#define SQL_BLOB 13
#include "odbc_zos_params.h"

struct mock {
    size_t remaining, read_calls, put_calls, sent, zero_puts;
    size_t fail_read_at, fail_put_at;
    int oversized_read;
    const char *expected;
};

static ptrdiff_t mock_read(void *context, char *buffer, size_t capacity)
{
    struct mock *m = context;
    size_t amount;
    ++m->read_calls;
    if (m->read_calls == m->fail_read_at) {
        return -1;
    }
    if (m->oversized_read) {
        return (ptrdiff_t) capacity + 1;
    }
    amount = m->remaining < capacity ? m->remaining : capacity;
    m->remaining -= amount;
    /* No need to generate/read gigabytes in the cumulative limit test. */
    (void) buffer;
    return (ptrdiff_t) amount;
}

static int mock_put(void *context, const char *buffer, SQLLEN length)
{
    struct mock *m = context;
    assert(length >= 0 && length <= PDO_ODBC_ZOS_PUT_CHUNK);
    ++m->put_calls;
    if (m->put_calls == m->fail_put_at) {
        return 0;
    }
    if (m->expected) {
        assert(memcmp(m->expected + m->sent, buffer, (size_t) length) == 0);
    }
    m->sent += (size_t) length;
    m->zero_puts += length == 0;
    return 1;
}

static void test_lengths(void)
{
    SQLLEN length = 123, capacity;
    size_t payload, amount;
    const size_t max = (size_t) INT32_MAX;
    assert(sizeof(SQLLEN) == 4);
    assert(pdo_odbc_zos_length(0, &length) && length == 0);
    assert(pdo_odbc_zos_length(max, &length) && length == INT32_MAX);
    assert(!pdo_odbc_zos_length(max + 1, &length) && length == INT32_MAX);
    assert(!pdo_odbc_zos_length(SIZE_MAX, &length));

    assert(pdo_odbc_zos_capacity(0, 0, &payload, &capacity));
    assert(payload == 0 && capacity == 1);
    assert(pdo_odbc_zos_capacity(max - 1, 0, &payload, &capacity));
    assert(payload == max - 1 && capacity == INT32_MAX);
    assert(!pdo_odbc_zos_capacity(max, 0, &payload, &capacity));
    assert(!pdo_odbc_zos_capacity(SIZE_MAX, 0, &payload, &capacity));
    assert(pdo_odbc_zos_capacity((max - 2) / 2, 1, &payload, &capacity));
    assert(payload == ((max - 2) / 2) * 2 && capacity == (SQLLEN) payload + 2);
    assert(!pdo_odbc_zos_capacity((max - 2) / 2 + 1, 1, &payload, &capacity));
    assert(!pdo_odbc_zos_capacity(SIZE_MAX, 1, &payload, &capacity));

    assert(pdo_odbc_zos_indicator(SQL_NULL_DATA, 0));
    assert(pdo_odbc_zos_indicator(0, 0));
    assert(pdo_odbc_zos_indicator(4, 4));
    assert(!pdo_odbc_zos_indicator(5, 4));
    assert(!pdo_odbc_zos_indicator(SQL_NO_TOTAL, 1024));
    assert(!pdo_odbc_zos_indicator(-2, 1024));
    assert(pdo_odbc_zos_indicator(INT32_MAX, max));
    assert(pdo_odbc_zos_name_length(0, 127));
    assert(pdo_odbc_zos_name_length(126, 127));
    assert(!pdo_odbc_zos_name_length(127, 127));
    assert(!pdo_odbc_zos_name_length(128, 127));
    assert(!pdo_odbc_zos_name_length(-1, 127));

    assert(pdo_odbc_zos_result_chunk(256, 0, 256, 0, &amount) && amount == 256);
    assert(!pdo_odbc_zos_result_chunk(256, 0, 255, 0, &amount));
    assert(pdo_odbc_zos_result_chunk(SQL_NO_TOTAL, 1, 256, 0, &amount) && amount == 256);
    assert(!pdo_odbc_zos_result_chunk(SQL_NO_TOTAL, 0, 256, 0, &amount));
    assert(!pdo_odbc_zos_result_chunk(SQL_NULL_DATA, 0, 256, 0, &amount));
    assert(!pdo_odbc_zos_result_chunk(255, 1, 256, 0, &amount));
    assert(pdo_odbc_zos_result_chunk(1024, 1, 256, 0, &amount) && amount == 256);
    assert(pdo_odbc_zos_result_chunk(1, 0, 256, max - 1, &amount));
    assert(!pdo_odbc_zos_result_chunk(2, 0, 256, max - 1, &amount));
    assert(!pdo_odbc_zos_result_chunk(0, 0, 256, SIZE_MAX, &amount));
}

static void test_classification(void)
{
    assert(pdo_odbc_zos_binary_input(SQL_BLOB));
    assert(pdo_odbc_zos_blob(SQL_BLOB));
    assert(pdo_odbc_zos_binary_input(SQL_BINARY));
    assert(pdo_odbc_zos_binary_input(SQL_VARBINARY));
    assert(pdo_odbc_zos_binary_input(SQL_LONGVARBINARY));
    /* Only native BLOB uses binary result chunks, preserving standard hex. */
    assert(!pdo_odbc_zos_blob(SQL_BINARY));
    assert(!pdo_odbc_zos_blob(SQL_VARBINARY));
    assert(!pdo_odbc_zos_blob(SQL_LONGVARBINARY));
    assert(!pdo_odbc_zos_binary_input(99));
}

static void test_strings(void)
{
    char data[20000];
    struct mock m = {0};
    memset(data, 'x', sizeof(data));
    data[8192] = '\0'; /* binary-safe, including embedded NUL */
    m.expected = data;
    assert(pdo_odbc_zos_put_string(data, sizeof(data), mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.put_calls == 3 && m.sent == sizeof(data) && m.zero_puts == 0);
    m = (struct mock) {0};
    assert(pdo_odbc_zos_put_string("", 0, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.put_calls == 1 && m.zero_puts == 1);
    m = (struct mock) {.fail_put_at = 1};
    assert(pdo_odbc_zos_put_string("", 0, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_PUT);
    assert(m.put_calls == 1 && m.sent == 0);
    m = (struct mock) {.fail_put_at = 2};
    assert(pdo_odbc_zos_put_string(data, sizeof(data), mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_PUT);
    assert(m.put_calls == 2 && m.sent == 8192);
    m = (struct mock) {0};
    assert(pdo_odbc_zos_put_string(data, PDO_ODBC_ZOS_MAX_LEN + 1, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_LENGTH);
    assert(m.put_calls == 0); /* rejected before touching the pointer */
}

static void test_streams(void)
{
    struct mock m = {0};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.put_calls == 1 && m.zero_puts == 1 && m.read_calls == 1);
    m = (struct mock) {.remaining = 16384};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.sent == 16384 && m.put_calls == 2 && m.zero_puts == 0 && m.read_calls == 3);
    m = (struct mock) {.remaining = 8193};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.sent == 8193 && m.put_calls == 2 && m.zero_puts == 0);
    m = (struct mock) {.remaining = 20000, .fail_read_at = 1};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_READ);
    assert(m.put_calls == 0 && m.read_calls == 1);
    m = (struct mock) {.remaining = 20000, .fail_read_at = 2};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_READ);
    assert(m.put_calls == 1 && m.sent == 8192 && m.read_calls == 2);
    m = (struct mock) {.remaining = 20000, .fail_put_at = 1};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_PUT);
    assert(m.put_calls == 1 && m.read_calls == 1);
    m = (struct mock) {.fail_put_at = 1};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_PUT);
    assert(m.put_calls == 1 && m.read_calls == 1);
    m = (struct mock) {.oversized_read = 1};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_READ);
    assert(m.put_calls == 0);
    m = (struct mock) {.remaining = PDO_ODBC_ZOS_MAX_LEN};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_OK);
    assert(m.sent == PDO_ODBC_ZOS_MAX_LEN && m.zero_puts == 0);
    m = (struct mock) {.remaining = PDO_ODBC_ZOS_MAX_LEN + 1};
    assert(pdo_odbc_zos_put_stream(mock_read, &m, mock_put, &m) == PDO_ODBC_ZOS_TRANSFER_LENGTH);
    assert(m.sent == (PDO_ODBC_ZOS_MAX_LEN / 8192) * 8192 && m.zero_puts == 0);
}

/* Minimal PHP API mocks for the extracted, actual native EXEC_PRE function.
 * Streams deliberately have no stat interface. No PHP or ODBC build needed. */
enum {
    IS_NULL, IS_STRING, IS_RESOURCE, PDO_PARAM_NULL = 20,
    PDO_PARAM_LOB, PDO_PARAM_STR, SQL_PARAM_INPUT, SQL_PARAM_OUTPUT,
    SQL_PARAM_INPUT_OUTPUT, SQL_DATA_AT_EXEC = -2
};
typedef struct { int valid; } php_stream;
typedef struct { int type; char *data; size_t length; php_stream *stream; } zval;
typedef struct {
    SQLLEN len;
    int paramtype;
    char *outbuf;
    size_t outbuf_payload;
} pdo_odbc_param;
typedef struct { void *dbh; void *driver_data; } pdo_stmt_t;
struct pdo_bound_param_data {
    pdo_odbc_param *driver_data;
    zval parameter;
    int param_type;
};
#define Z_ISREF(value) 0
#define Z_REFVAL(value) (&(value))
#define Z_TYPE_P(value) ((value)->type)
#define Z_STRLEN_P(value) ((value)->length)
#define Z_STRVAL_P(value) ((value)->data)
#define PDO_PARAM_TYPE(value) (value)
#define php_stream_from_zval_no_verify(result, value) \
    ((result) = (value)->stream && (value)->stream->valid ? (value)->stream : NULL)
static void convert_to_string(zval *value) { assert(value->type == IS_STRING); }
static int pre_errors;
static const char *last_error_state;
static void pdo_raise_impl_error(void *dbh, pdo_stmt_t *stmt, const char *state, const char *message)
{
    (void) dbh;
    (void) stmt;
    (void) message;
    assert(strcmp(state, "HY090") == 0 || strcmp(state, "HY000") == 0 ||
        strcmp(state, "22001") == 0 || strcmp(state, "IM001") == 0);
    last_error_state = state;
    ++pre_errors;
}
#include "param-pre-under-test.h"

static void test_actual_exec_pre(void)
{
    pdo_stmt_t stmt = {0};
    pdo_odbc_param p = {.paramtype = SQL_PARAM_INPUT};
    struct pdo_bound_param_data param = {
        .driver_data = &p, .param_type = PDO_PARAM_LOB,
        .parameter = {.type = IS_NULL}
    };
    php_stream stream = {.valid = 1};
    char storage[6] = "guard";
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == SQL_NULL_DATA);
    param.parameter = (zval) {.type = IS_STRING, .data = "", .length = 0};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == SQL_DATA_AT_EXEC);
    param.parameter = (zval) {.type = IS_RESOURCE, .stream = &stream};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == SQL_DATA_AT_EXEC);
    stream.valid = 0;
    assert(!pdo_odbc_zos_param_pre(&stmt, &param) && pre_errors == 1);
    param.param_type = PDO_PARAM_STR;
    param.parameter = (zval) {.type = IS_STRING, .data = "unused", .length = PDO_ODBC_ZOS_MAX_LEN};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == SQL_DATA_AT_EXEC);
    ++param.parameter.length;
    assert(!pdo_odbc_zos_param_pre(&stmt, &param) && pre_errors == 2);

    p.paramtype = SQL_PARAM_INPUT_OUTPUT;
    p.outbuf = storage;
    p.outbuf_payload = 4;
    param.parameter = (zval) {.type = IS_STRING, .data = "abcd", .length = 4};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == 4);
    assert(memcmp(storage, "abcd\0", 5) == 0);
    param.parameter = (zval) {.type = IS_STRING, .data = "", .length = 0};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == 0);
    param.parameter = (zval) {.type = IS_STRING, .data = "abcd", .length = 4};
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == 4); /* capacity did not shrink */
    param.parameter = (zval) {.type = IS_STRING, .data = "oversize", .length = 8};
    assert(!pdo_odbc_zos_param_pre(&stmt, &param) && pre_errors == 3);
    assert(memcmp(storage, "abcd\0", 5) == 0 && storage[5] == '\0');
    p.paramtype = SQL_PARAM_OUTPUT;
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == 0); /* no input copy */
    param.param_type = PDO_PARAM_NULL;
    assert(pdo_odbc_zos_param_pre(&stmt, &param) && p.len == SQL_NULL_DATA);
}

/* Mock Zend strings and SQLGetData for the actual native result reader. */
enum { SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_NO_DATA, SQL_ERROR, SQL_C_BINARY, SQL_C_CHAR };
typedef int RETCODE;
typedef struct { char *data; size_t length; } zend_string;
#define ZSTR_VAL(s) ((s)->data)
static zend_string *zend_string_init(const char *data, size_t length, int persistent)
{
    zend_string *s = malloc(sizeof(*s));
    (void) persistent;
    assert(s && length <= 2048);
    s->data = malloc(length + 1);
    assert(s->data);
    memcpy(s->data, data, length);
    s->data[length] = '\0';
    s->length = length;
    return s;
}
static zend_string *zend_string_realloc(zend_string *s, size_t length, int persistent)
{
    (void) persistent;
    assert(length <= 2048);
    s->data = realloc(s->data, length + 1);
    assert(s->data);
    s->length = length;
    return s;
}
static void zend_string_release(zend_string *s) { free(s->data); free(s); }
static void mock_zval_str(zval *result, zend_string *s)
{
    result->type = IS_STRING;
    result->data = s->data;
    result->length = s->length;
    free(s);
}
#define ZVAL_STR(result, s) mock_zval_str(result, s)
#define ZVAL_STRINGL(result, data, length) mock_zval_str(result, zend_string_init(data, length, 0))
#define ZVAL_NULL(result) ((result)->type = IS_NULL)
typedef struct {
    char *data;
    size_t datalen;
    SQLLEN fetched_len;
    SQLSMALLINT coltype;
    int is_long, is_unicode;
} pdo_odbc_column;
typedef struct { int stmt; pdo_odbc_column *cols; unsigned assume_utf8; } pdo_odbc_stmt;
struct get_step { int rc; SQLLEN indicator; size_t bytes; char fill; };
static struct get_step get_steps[3];
static size_t get_step_count, get_calls;
static const char *get_source;
static int get_expected_ctype, get_errors;
#define pdo_odbc_stmt_error(what) do { (void) (what); ++get_errors; } while (0)
static RETCODE SQLGetData(int statement, int column, int ctype, char *data, SQLLEN capacity, SQLLEN *indicator)
{
    struct get_step step;
    (void) statement;
    assert(column == 1 && capacity == 256 && ctype == get_expected_ctype);
    assert(get_calls < get_step_count);
    step = get_steps[get_calls++];
    assert(step.bytes <= (size_t) capacity);
    /* The actual reader must clear scratch storage on every call. */
    for (size_t i = 0; i < (size_t) capacity; ++i) assert(data[i] == '\0');
    if (get_source) {
        memcpy(data, get_source, step.bytes);
    } else {
        memset(data, step.fill, step.bytes);
    }
    if (ctype == SQL_C_CHAR && step.bytes < (size_t) capacity) {
        data[step.bytes] = '\0';
    }
    *indicator = step.indicator;
    return step.rc;
}
#include "param-col-under-test.h"

static void test_actual_get_col(void)
{
    char buffer[256];
    pdo_odbc_column col = {.data = buffer, .coltype = SQL_BLOB, .is_long = 1};
    pdo_odbc_stmt driver = {.cols = &col};
    pdo_stmt_t stmt = {.driver_data = &driver};
    zval result = {.type = IS_RESOURCE};
    size_t i;
    int errors;
    get_expected_ctype = SQL_C_BINARY;
    get_steps[0] = (struct get_step) {SQL_SUCCESS_WITH_INFO, 300, 256, '\0'};
    get_steps[1] = (struct get_step) {SQL_SUCCESS, 44, 44, 'x'};
    get_step_count = 2;
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.type == IS_STRING && result.length == 300 && get_calls == 2);
    for (i = 0; i < 300; ++i) assert(result.data[i] == (i < 256 ? '\0' : 'x'));
    free(result.data);

    col.coltype = SQL_VARBINARY; /* preserve SQL_C_CHAR hex contract */
    get_expected_ctype = SQL_C_CHAR;
    get_steps[0] = (struct get_step) {SQL_SUCCESS_WITH_INFO, 300, 255, 'A'};
    get_steps[1] = (struct get_step) {SQL_SUCCESS, 45, 45, 'B'};
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.length == 300 && get_calls == 2);
    for (i = 0; i < 300; ++i) assert(result.data[i] == (i < 255 ? 'A' : 'B'));
    free(result.data);

    /* A truncated UTF-8 character chunk can stop short of 255 bytes.
     * Never append the stale tail or zero padding, or guess via strlen. */
    memset(buffer, 'S', sizeof(buffer));
    get_steps[0] = (struct get_step) {SQL_SUCCESS_WITH_INFO, 300, 252, 'U'};
    get_step_count = 1;
    get_calls = 0;
    result = (zval) {.type = IS_RESOURCE, .data = "untouched", .length = 9};
    errors = pre_errors;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(get_calls == 1 && pre_errors == errors + 1);
    assert(strcmp(last_error_state, "22001") == 0);
    assert(result.type == IS_RESOURCE && result.length == 9 && strcmp(result.data, "untouched") == 0);
    assert(buffer[251] == 'U' && buffer[252] == '\0' && buffer[255] == '\0');
    get_steps[0].indicator = SQL_NO_TOTAL; /* same ambiguity with unknown total */
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && get_calls == 1);

    /* Full-size truncation with an embedded NUL is also ambiguous. */
    {
        char text[255];
        memset(text, 'T', sizeof(text));
        text[10] = '\0';
        get_source = text;
        get_steps[0] = (struct get_step) {SQL_SUCCESS_WITH_INFO, 300, 255, 0};
        get_calls = 0;
        assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && get_calls == 1);
        get_source = NULL;
    }
    /* A final known-length text chunk preserves embedded NUL and its suffix. */
    get_source = "A\0B";
    get_steps[0] = (struct get_step) {SQL_SUCCESS, 3, 3, 0};
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.type == IS_STRING && result.length == 3 && memcmp(result.data, "A\0B", 3) == 0);
    free(result.data);
    get_source = NULL;
    result.type = IS_RESOURCE;
    get_steps[0] = (struct get_step) {SQL_SUCCESS, 255, 256, 'N'};
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_RESOURCE); /* no terminator */
    get_steps[0] = (struct get_step) {SQL_SUCCESS, SQL_NO_TOTAL, 3, 'F'};
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_RESOURCE); /* unknown final text */

    col.coltype = SQL_BLOB;
    get_expected_ctype = SQL_C_BINARY;
    get_step_count = 2;
    get_steps[0] = (struct get_step) {SQL_SUCCESS_WITH_INFO, SQL_NO_TOTAL, 256, 'C'};
    get_steps[1] = (struct get_step) {SQL_SUCCESS, 3, 3, 'D'};
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result) && result.length == 259);
    assert(result.data[255] == 'C' && result.data[256] == 'D');
    free(result.data);

    get_step_count = 1;
    get_steps[0] = (struct get_step) {SQL_SUCCESS, 0, 0, 0};
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.type == IS_STRING && result.length == 0);
    free(result.data);
    get_steps[0].indicator = SQL_NULL_DATA;
    get_calls = 0;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_NULL);

    result.type = IS_RESOURCE;
    errors = pre_errors;
    get_steps[0].indicator = SQL_NO_TOTAL;
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.type == IS_RESOURCE && pre_errors == errors + 1);
    get_steps[0].indicator = 257;
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_RESOURCE);
    get_steps[0].rc = SQL_ERROR;
    get_calls = 0;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && get_errors == 1);

    col.is_long = 0;
    col.datalen = 4;
    col.fetched_len = 5;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_RESOURCE);
    col.fetched_len = SQL_NO_TOTAL;
    assert(!pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_RESOURCE);
    col.fetched_len = SQL_NULL_DATA;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result) && result.type == IS_NULL);
    memset(buffer, 'E', 4);
    col.fetched_len = 4;
    assert(pdo_odbc_zos_get_col(&stmt, 0, &result));
    assert(result.length == 4 && memcmp(result.data, "EEEE", 4) == 0);
    free(result.data);
}

static int zval_is_true(zval *value)
{
    assert(value->type == IS_STRING);
    return value->length != 0; /* tests below use only "1" and "" */
}
static int actual_statement_assume_utf8(pdo_stmt_t *stmt, zval *val)
{
    pdo_odbc_stmt *S = stmt->driver_data;
#include "param-attribute-under-test.h"
}
static void test_actual_statement_attribute(void)
{
    pdo_odbc_stmt driver = {0};
    pdo_stmt_t stmt = {.driver_data = &driver};
    zval value = {.type = IS_STRING, .data = "1", .length = 1};
    int errors = pre_errors;
    assert(!actual_statement_assume_utf8(&stmt, &value));
    assert(driver.assume_utf8 == 0 && pre_errors == errors + 1);
    assert(strcmp(last_error_state, "IM001") == 0);
    /* Rejection must not mutate the old state, even for a legacy true value. */
    driver.assume_utf8 = 1;
    assert(!actual_statement_assume_utf8(&stmt, &value) && driver.assume_utf8 == 1);
    value.data = "";
    value.length = 0;
    errors = pre_errors;
    assert(actual_statement_assume_utf8(&stmt, &value));
    assert(driver.assume_utf8 == 0 && pre_errors == errors);
}

int main(void)
{
    test_actual_statement_attribute();
    test_actual_get_col();
    pre_errors = 0;
    test_actual_exec_pre();
    test_lengths();
    test_classification();
    test_strings();
    test_streams();
    puts("native parameter helpers: PASS (32-bit SQLLEN, no SDK or database)");
    return 0;
}
