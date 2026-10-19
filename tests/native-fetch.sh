#!/bin/sh
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
src=${1:-"$port/php-8.2.34"}
tmp=$(mktemp -d /tmp/phpport-fetch-test.XXXXXX)
trap 'rm -rf "$tmp"' 0 HUP INT TERM
: > "$tmp/fetch-under-test.h"
if command -v chtag >/dev/null 2>&1; then chtag -tc ISO8859-1 "$tmp/fetch-under-test.h"; fi
awk '
  /^static int odbc_stmt_fetch\(/{copy=1}
  /^static int odbc_stmt_describe\(/{copy=0}
  copy{print}
' "$src/ext/pdo_odbc/odbc_stmt.c" > "$tmp/fetch-under-test.h"
grep -q 'static int odbc_stmt_fetch' "$tmp/fetch-under-test.h"
flags=
case "$(uname)" in OS/390) flags='-fzos-le-char-mode=ascii -D_ALL_SOURCE -D_XOPEN_SOURCE=600 -D_AE_BIMODAL=1' ;; esac
"${CC:-clang}" $flags -std=c11 -I"$tmp" "$port/tests/fetch-harness.c" -o "$tmp/fetch-test"
"$tmp/fetch-test"
