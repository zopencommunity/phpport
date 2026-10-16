#!/bin/sh
# Trusted private environment only. Never put credentials in arguments or Git.
set +x
set -eu
umask 077
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
if [ "$#" -ne 3 ]; then
  echo 'Usage: sh tests/db2-acceptance.sh /private/env.sh /absolute/bin/php /absolute/bin/php-cgi' >&2
  exit 2
fi
env_file=$1
php=$2
cgi=$3
for path in "$env_file" "$php" "$cgi"; do
  case "$path" in /*) ;; *) echo 'All paths must be absolute.' >&2; exit 2 ;; esac
done
[ -r "$env_file" ] && [ -f "$env_file" ] || { echo 'Private environment file is not readable.' >&2; exit 77; }
[ -x "$php" ] && [ -x "$cgi" ] || { echo 'Both candidate CLI and CGI executables are required.' >&2; exit 77; }
command -v timeout >/dev/null || { echo 'coreutils timeout is required.' >&2; exit 77; }
out=$(mktemp -d "${TMPDIR:-/tmp}/php-db2-acceptance.XXXXXX")
for name in env-load.log cli.out cli.err cgi.out cgi.err; do
  : > "$out/$name"
  chtag -t -c ISO8859-1 "$out/$name"
done
# The environment may use this private path for native trace/dump destinations.
export PHP_DB2_TEST_REPORT_DIR="$out"
# Even a trusted file's accidental tracing/output remains in the private log.
. "$env_file" > "$out/env-load.log" 2>&1
set +x
set -eu
umask 077
export PHP_DB2_TEST_REPORT_DIR="$out"
export USE_ZEND_ALLOC=1
if [ -n "${DSNAOINI:-}" ] && [ -f "$DSNAOINI" ] && [ -r "$DSNAOINI" ]; then
  if ! chtag -p "$DSNAOINI" | grep -q 'T=off'; then
    echo 'REFUSED: DSNAOINI must have EBCDIC bytes and T=off; retagging alone is not conversion.' >&2
    exit 77
  fi
  if grep -Fq '[COMMON]' "$DSNAOINI"; then
    echo 'REFUSED: DSNAOINI appears to contain ASCII initialization bytes; verify/convert it privately.' >&2
    exit 77
  fi
fi
printf 'Private reports: %s\n' "$out"
# Default native dumps and working files must not land in a shared workspace.
cd "$out"
cli_rc=0
timeout --kill-after=10 300 "$php" -n "$port/tests/db2-acceptance.php" > "$out/cli.out" 2> "$out/cli.err" || cli_rc=$?
# Do not echo raw diagnostics: native traces may disclose SQL or connection data.
if [ "$cli_rc" -ne 0 ]; then
  printf 'CLI acceptance did not pass (exit %s); inspect private reports.\n' "$cli_rc" >&2
  exit "$cli_rc"
fi
"$php" -n -r '$r=json_decode(file_get_contents($argv[1]),true); if(!is_array($r)||($r["ok"]??false)!==true||($r["sapi"]??"")!=="cli") exit(1); echo "CLI live Db2 checks passed: ",count($r["checks"]),"\n";' "$out/cli.out"
token=$("$php" -n -r 'echo bin2hex(random_bytes(24));')
export PHP_DB2_ACCEPTANCE_TOKEN="$token" HTTP_X_PHP_DB2_ACCEPTANCE_TOKEN="$token"
export REQUEST_METHOD=GET REDIRECT_STATUS=1 REMOTE_ADDR=127.0.0.1 SERVER_NAME=localhost
export GATEWAY_INTERFACE=CGI/1.1 SERVER_PROTOCOL=HTTP/1.1 SCRIPT_FILENAME="$port/tests/db2-acceptance.php"
cgi_rc=0
timeout --kill-after=10 300 "$cgi" -n > "$out/cgi.out" 2> "$out/cgi.err" || cgi_rc=$?
if [ "$cgi_rc" -ne 0 ]; then
  printf 'CGI acceptance did not pass (exit %s); inspect private reports.\n' "$cgi_rc" >&2
  exit "$cgi_rc"
fi
"$php" -n -r '$s=file_get_contents($argv[1]); $p=preg_split("/\\r?\\n\\r?\\n/",$s,2); $r=json_decode($p[1]??"",true); if(!is_array($r)||($r["ok"]??false)!==true||($r["sapi"]??"")!=="cgi-fcgi") exit(1); echo "CGI live Db2 checks passed: ",count($r["checks"]),"\n";' "$out/cgi.out"
echo 'PASS: CLI and CGI live Db2 acceptance completed, including owned-table cleanup.'
