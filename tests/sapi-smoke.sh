#!/bin/sh
# Exercise the PHP binaries actually built, without opening a Db2 connection.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
src=$(CDPATH= cd -- "$1" && pwd -P)
mode=$2
script="$port/tests/sapi-smoke.php"
cli="$src/sapi/cli/php"
case "$mode" in
  static)
    result=$("$cli" -n "$script")
    ;;
  shared)
    export LIBPATH="$src/sapi/cli${LIBPATH:+:${LIBPATH}}"
    result=$("$cli" -n -d "extension=$src/modules/pdo_odbc.so" "$script")
    ;;
  *) echo 'Expected static or shared PDO_ODBC mode' >&2; exit 1 ;;
esac
printf '%s\n' "$result"
printf '%s\n' "$result" | grep -qx 'PDO_ODBC cli OK'

# Shared mode is explicitly restricted to CLI. Built-in mode must work in
# every additional SAPI produced by this build, not import a separate core.
if [ "$mode" = static ]; then
  cgi="$src/sapi/cgi/php-cgi"
  if [ -x "$cgi" ]; then
    result=$(REQUEST_METHOD=GET SCRIPT_FILENAME="$script" REDIRECT_STATUS=1 \
      GATEWAY_INTERFACE=CGI/1.1 SERVER_PROTOCOL=HTTP/1.1 SERVER_NAME=localhost \
      "$cgi" -n)
    printf '%s\n' "$result"
    printf '%s\n' "$result" | tr -d '\r' | grep -qx 'PDO_ODBC cgi-fcgi OK'
  fi
  dbg="$src/sapi/phpdbg/phpdbg"
  if [ -x "$dbg" ]; then
    result=$("$dbg" -n -qrr "$script")
    printf '%s\n' "$result"
    printf '%s\n' "$result" | grep -qx 'PDO_ODBC phpdbg OK'
  fi
fi
