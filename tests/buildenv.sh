#!/bin/sh
# No compiler, Db2 SDK, or database connection is needed for these checks.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d /tmp/phpport-buildenv.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

ZOPEN_PDO_ODBC=no
. "$port/buildenv"
case "$ZOPEN_EXTRA_CONFIGURE_OPTS" in
  *pdo-odbc*) echo 'Default build unexpectedly enables PDO_ODBC' >&2; exit 1 ;;
esac

TESTS=ext/pdo_odbc
printf '%s\n' \
  'Tests skipped   : 85 (91.4%)' \
  'Tests warned    : 0 (0.0%)' \
  'Tests failed    : 0 (0.0%)' \
  'Tests passed    : 8 (8.6%)' > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
for expected in actualPasses:8 actualSkips:85 actualFailures:0 expectedFailures:0 totalTests:8; do
  printf '%s\n' "$result" | grep -qx "$expected"
done

printf '%s\n' \
  'Tests skipped   : 4 (33.3%)' \
  'Tests warned    : 1 (8.3%)' \
  'Tests borked    : 1 (8.3%)' \
  'Tests failed    : 2 (16.7%)' \
  'Tests passed    : 4 (33.3%)' > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
printf '%s\n' "$result" | grep -qx 'actualFailures:4'
printf '%s\n' "$result" | grep -qx 'totalTests:8'

# An absent/incomplete runner summary must not manufacture passing tests.
: > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
printf '%s\n' "$result" | grep -qx 'totalTests:0'

ZOPEN_PDO_ODBC=shared
. "$port/buildenv"
case "$ZOPEN_EXTRA_CONFIGURE_OPTS" in
  *--disable-cgi*--disable-phpdbg*--with-pdo-odbc=shared,ibm-db2-zos*) ;;
  *) echo 'Missing native shared configuration flags' >&2; exit 1 ;;
esac
[ "$ZOPEN_BOOTSTRAP" = ./buildconf ]
[ "$ZOPEN_BOOTSTRAP_OPTS" = --force ]

if (ZOPEN_PDO_ODBC=invalid; . "$port/buildenv") 2>/dev/null; then
  echo 'Invalid PDO_ODBC build mode was accepted' >&2
  exit 1
fi
printf '%s\n' 'buildenv checks passed'
