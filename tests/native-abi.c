/* Verify actual site SDK typedefs without calling any Db2 function. */
#include <stdio.h>
#include <sqlcli1.h>
#include "odbc_zos_params.h"
int main(void)
{
    printf("Native ABI: pointers=%zu SQLPOINTER=%zu handles=%zu/%zu/%zu SQLLEN=%zu SQLULEN=%zu\n",
        sizeof(void *), sizeof(SQLPOINTER), sizeof(SQLHENV), sizeof(SQLHDBC),
        sizeof(SQLHSTMT), sizeof(SQLLEN), sizeof(SQLULEN));
    if (sizeof(void *) != 8 || sizeof(SQLPOINTER) != 8 || sizeof(SQLHENV) != 4 ||
        sizeof(SQLHDBC) != 4 || sizeof(SQLHSTMT) != 4 || sizeof(SQLLEN) != 4 || sizeof(SQLULEN) != 4) {
        fputs("Unexpected native CLI ABI; review the SDK before packaging.\n", stderr);
        return 1;
    }
#ifdef SQL_BLOB
    if (!pdo_odbc_zos_blob(SQL_BLOB)) {
        fputs("SDK has no distinct native BLOB type; raw BLOB handling needs review.\n", stderr);
        return 1;
    }
#else
    fputs("Native SDK SQL_BLOB declaration is required.\n", stderr);
    return 1;
#endif
    puts("Native SDK ABI and BLOB classification: PASS (no database calls)");
    return 0;
}
