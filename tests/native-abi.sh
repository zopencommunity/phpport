#!/bin/sh
# Site SDK declarations only: do not link/call the IBM runtime for this probe.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
src=${1:-"$port/php-8.2.34"}
: "${PDO_ODBC_DB2_INCLUDE:?Native SDK include directory is required}"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/phpport-native-abi.XXXXXX")
trap 'rm -rf "$tmp"' 0 HUP INT TERM
"${CC:-clang}" -std=c11 -Wno-trigraphs -m64 -fzos-le-char-mode=ascii -D_ALL_SOURCE -D_XOPEN_SOURCE=600 -D_AE_BIMODAL=1 \
  -I"$PDO_ODBC_DB2_INCLUDE" -I"$src/ext/pdo_odbc" "$port/tests/native-abi.c" -o "$tmp/native-abi"
"$tmp/native-abi"
