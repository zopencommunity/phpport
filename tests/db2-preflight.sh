#!/bin/sh
# Refusal-path tests only: no Db2 library calls or database writes are allowed.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
php=${1:-php}
cgi=${2:-}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/php-db2-preflight.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for name in result.json error.log; do
  : > "$tmp/$name"
  if command -v chtag >/dev/null 2>&1; then chtag -t -c ISO8859-1 "$tmp/$name"; fi
done
"$php" -n "$port/tests/db2-manifest.php" "$tmp"
# Native sh returns nonzero when unsetting an absent variable. Explicit
# non-configured fixtures also prevent inherited site settings being used.
export DSNAOINI="$tmp/not-provided.ini" PDO_ODBC_TEST_USER= PDO_ODBC_TEST_PASS=
export PDO_ODBC_TEST_DSN=odbc:deliberately-unconfigured
export PDO_ODBC_TEST_SCHEMA=ZOPEN_TEST PHP_DB2_TEST_ALLOW_WRITES=yes PHP_DB2_TEST_REPORT_DIR="$tmp"
run_refusal()
{
  expected=$1
  if "$php" -n "$port/tests/db2-acceptance.php" > "$tmp/result.json" 2> "$tmp/error.log"; then rc=0; else rc=$?; fi
  [ "$rc" -eq 77 ] || { echo 'Acceptance refusal did not exit 77' >&2; exit 1; }
  "$php" -n -r '$r=json_decode(file_get_contents($argv[1]),true); if(!is_array($r)||($r["ok"]??true)!==false||($r["checks"]??null)!==[]||!preg_match($argv[2],$r["refused"]??""))exit(1);' "$tmp/result.json" "$expected"
}
PHP_DB2_TEST_ALLOW_WRITES=no; export PHP_DB2_TEST_ALLOW_WRITES
run_refusal '~^explicit-write-approval-required$~'
PHP_DB2_TEST_ALLOW_WRITES=yes; PDO_ODBC_TEST_SCHEMA=SYSIBM
export PHP_DB2_TEST_ALLOW_WRITES PDO_ODBC_TEST_SCHEMA
run_refusal '~^approved-disposable-schema-required$~'
PDO_ODBC_TEST_SCHEMA=ZOPEN_TEST; PDO_ODBC_TEST_DSN=; export PDO_ODBC_TEST_SCHEMA PDO_ODBC_TEST_DSN
run_refusal '~^private-odbc-dsn-required$~'
PDO_ODBC_TEST_DSN=odbc:deliberately-unconfigured; PHP_DB2_TEST_REPORT_DIR=
export PDO_ODBC_TEST_DSN PHP_DB2_TEST_REPORT_DIR
run_refusal '~^private-report-directory-required$~'
PHP_DB2_TEST_REPORT_DIR="$tmp"; export PHP_DB2_TEST_REPORT_DIR
run_refusal '~^(built-in-pdo-odbc-required|approved-readable-uss-dsnaoini-required)$~'
if [ -n "$cgi" ] && [ -x "$cgi" ]; then
  : > "$tmp/cgi.out"
  if command -v chtag >/dev/null 2>&1; then chtag -t -c ISO8859-1 "$tmp/cgi.out"; fi
  # Missing write approval remains a second independent barrier if auth breaks.
  export PHP_DB2_TEST_ALLOW_WRITES=no REQUEST_METHOD=GET REDIRECT_STATUS=1
  export GATEWAY_INTERFACE=CGI/1.1 SERVER_PROTOCOL=HTTP/1.1 SERVER_NAME=localhost
  export SCRIPT_FILENAME="$port/tests/db2-acceptance.php" REMOTE_ADDR=127.0.0.1
  export PHP_DB2_ACCEPTANCE_TOKEN=0123456789abcdef0123456789abcdef
  run_cgi_refusal()
  {
    expected=$1
    if timeout --kill-after=5 30 "$cgi" -n > "$tmp/cgi.out" 2> "$tmp/error.log"; then rc=0; else rc=$?; fi
    [ "$rc" -eq 77 ] || { echo 'CGI refusal did not exit 77' >&2; exit 1; }
    "$php" -n -r '$s=file_get_contents($argv[1]);$p=preg_split("/\\r?\\n\\r?\\n/",$s,2);$r=json_decode($p[1]??"",true);if(!is_array($r)||($r["ok"]??true)!==false||($r["checks"]??null)!==[]||($r["refused"]??"")!==$argv[2])exit(1);' "$tmp/cgi.out" "$expected"
  }
  export HTTP_X_PHP_DB2_ACCEPTANCE_TOKEN=incorrect
  run_cgi_refusal private-cgi-runner-required
  HTTP_X_PHP_DB2_ACCEPTANCE_TOKEN=$PHP_DB2_ACCEPTANCE_TOKEN
  export HTTP_X_PHP_DB2_ACCEPTANCE_TOKEN REMOTE_ADDR=192.0.2.1
  run_cgi_refusal private-cgi-runner-required
  export REMOTE_ADDR=127.0.0.1
  run_cgi_refusal explicit-write-approval-required
  echo 'CGI token/loopback refusal checks passed.'
fi
echo 'Db2 acceptance refusal checks passed; no database operations performed.'
