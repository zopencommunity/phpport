#!/bin/sh
# DB-free: compile actual driver/helper excerpts against local, non-CLI mocks.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
src=${1:-"$port/php-8.2.34"}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/phpport-driver-test.XXXXXX")
trap 'rm -rf "$tmp"' 0 HUP INT TERM
header="$tmp/driver-under-test.h"
: > "$header"
if command -v chtag >/dev/null 2>&1; then chtag -tc ISO8859-1 "$header"; fi
awk '/^#define PDO_ODBC_STATE_SUCCEEDED\(rc\)/ {print; exit}' "$src/ext/pdo_odbc/odbc_driver.c" >> "$header"
awk '/^PHPAPI bool php_odbc_connstr_is_quoted/ {copy=1} copy {print}' "$src/main/php_odbc_utils.c" >> "$header"
for name in pdo_odbc_zos_complete_connstr odbc_handle_closer odbc_handle_doer odbc_handle_begin odbc_handle_commit odbc_handle_rollback odbc_handle_set_attr pdo_odbc_get_info_string pdo_odbc_handle_factory; do
  awk -v name="$name" '
    /^(static |PHPAPI )/ && index($0,name "(") {copy=1}
    copy {print}
    copy && /^}/ {exit}
  ' "$src/ext/pdo_odbc/odbc_driver.c" >> "$header"
done
flags=
case "$(uname)" in OS/390) flags='-fzos-le-char-mode=ascii -D_ALL_SOURCE -D_XOPEN_SOURCE=600 -D_AE_BIMODAL=1' ;; esac
# Intentional word splitting for the fixed, platform-specific compiler flags.
"${CC:-clang}" $flags -std=c11 -Wall -Wextra -Werror \
    -I"$src/ext/pdo_odbc" -I"$tmp" "$port/tests/driver-harness.c" -o "$tmp/driver-test"
"$tmp/driver-test"
