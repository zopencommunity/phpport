#!/bin/sh
# Database-free native SQLLEN/parameter helpers; no PHP build or SDK required.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
src=${1:-"$port/php-8.2.34"}
tmp=$(mktemp -d /tmp/phpport-native-params.XXXXXX)
trap 'rm -rf "$tmp"' 0 HUP INT TERM
for name in param-pre-under-test.h param-col-under-test.h param-attribute-under-test.h; do
    : > "$tmp/$name"
    if command -v chtag >/dev/null 2>&1; then chtag -tc ISO8859-1 "$tmp/$name"; fi
done
# Also compile the actual EXEC_PRE implementation against tiny PHP mocks.
awk '
  /^static int pdo_odbc_zos_param_pre\(/{copy=1}
  copy && /^#endif/{exit}
  copy{print}
' "$src/ext/pdo_odbc/odbc_stmt.c" > "$tmp/param-pre-under-test.h"
grep -q 'static int pdo_odbc_zos_param_pre' "$tmp/param-pre-under-test.h"
awk '
  /^static int pdo_odbc_zos_get_col\(/{copy=1}
  copy && /^#endif/{exit}
  copy{print}
' "$src/ext/pdo_odbc/odbc_stmt.c" > "$tmp/param-col-under-test.h"
grep -q 'static int pdo_odbc_zos_get_col' "$tmp/param-col-under-test.h"
# Extract just the statement setter's ASSUME_UTF8 case (not the getter).
awk '
  /^static int odbc_stmt_set_param\(/{setter=1}
  setter && /case PDO_ODBC_ATTR_ASSUME_UTF8:/{copy=1; next}
  copy && /default:/{exit}
  copy{print}
' "$src/ext/pdo_odbc/odbc_stmt.c" > "$tmp/param-attribute-under-test.h"
grep -q 'IM001' "$tmp/param-attribute-under-test.h"
flags=
case "$(uname)" in OS/390) flags='-fzos-le-char-mode=ascii -D_ALL_SOURCE -D_XOPEN_SOURCE=600 -D_AE_BIMODAL=1' ;; esac
"${CC:-clang}" $flags -std=c11 -Wall -Wextra -Werror -DPDO_ODBC_DB2_ZOS \
  -I"$src/ext/pdo_odbc" -I"$tmp" "$port/tests/param-harness.c" -o "$tmp/param-test"
"$tmp/param-test"
