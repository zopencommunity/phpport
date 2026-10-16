#!/bin/sh
# No compiler, Db2 SDK, or database connection is needed for these checks.
set -eu
port=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d /tmp/phpport-buildenv.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

ZOPEN_PDO_ODBC=no
. "$port/buildenv"
case "$ZOPEN_EXTRA_CONFIGURE_OPTS" in
  *pdo-odbc*) echo 'Explicit SDK-free build unexpectedly enables PDO_ODBC' >&2; exit 1 ;;
esac
case "$ZOPEN_EXTRA_CFLAGS" in
  *MAP_ANON*) echo 'Build must not invent anonymous mmap flag values' >&2; exit 1 ;;
esac

TESTS=ext/pdo_odbc
# A complete summary overrides individual result lines.
printf '%s\n' \
  'PASS partial result' \
  'FAIL partial result' \
  'Tests skipped   : 85 (91.4%)' \
  'Tests warned    : 0 (0.0%)' \
  'Tests failed    : 0 (0.0%)' \
  'Tests passed    : 8 (8.6%)' > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
for expected in actualPasses:8 actualSkips:85 actualFailures:0 expectedFailures:0 totalTests:8 testRunIncomplete:0; do
  printf '%s\n' "$result" | grep -qx "$expected"
  grep -qx "$expected" "$tmp/test_summary.log"
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

# One failure must exceed the focused-run allowance for every spelling.
# Paths need not exist: this exercises only summary parsing, without an SDK.
printf '%s\n' \
  'Tests skipped   : 0 (0.0%)' \
  'Tests warned    : 0 (0.0%)' \
  'Tests borked    : 0 (0.0%)' \
  'Tests failed    : 1 (100.0%)' \
  'Tests passed    : 0 (0.0%)' > "$tmp/test_check.log"
for TESTS in \
  'ext/pdo_odbc' \
  './ext/pdo_odbc' \
  '././ext/pdo_odbc/' \
  "$tmp/source/ext/pdo_odbc" \
  '-j4 ext/pdo_odbc' \
  '-j 4 ./ext/pdo_odbc' \
  '-j4 ./ext/pdo_odbc/' \
  "-j4 $tmp/source/ext/pdo_odbc" \
  'ext/pdo_odbc/tests/one.phpt' \
  './ext/pdo_odbc/tests/one.phpt' \
  "$tmp/source/ext/pdo_odbc/tests/one.phpt" \
  '-j4 ./ext/pdo_odbc/tests/one.phpt' \
  'ext/pdo_odbc/tests/one.phpt ./ext/pdo_odbc/tests/two.phpt' \
  '-d display_errors=1 --no-color ext/pdo_odbc'; do
  result=$(zopen_check_results "$tmp" test)
  printf '%s\n' "$result" | grep -qx 'actualFailures:1'
  printf '%s\n' "$result" | grep -qx 'expectedFailures:0'
  grep -qx 'expectedFailures:0' "$tmp/test_summary.log"
  failures=$(printf '%s\n' "$result" | awk -F: '/^actualFailures:/{print $2}')
  allowance=$(printf '%s\n' "$result" | awk -F: '/^expectedFailures:/{print $2}')
  [ "$failures" -gt "$allowance" ]
done

# Full/mixed suites retain the documented baseline, independent of ordering.
for TESTS in \
  '' '-j4' '.' 'ext' \
  'ext/pdo_odbc ext/pdo' \
  'ext/pdo ./ext/pdo_odbc' \
  "-j4 $tmp/source/ext/pdo_odbc ext/standard" \
  'ext/pdo_odbc_extra' \
  "$tmp/notext/pdo_odbc" \
  '-r tests.list ext/pdo_odbc'; do
  result=$(zopen_check_results "$tmp" test)
  printf '%s\n' "$result" | grep -qx 'actualFailures:1'
  printf '%s\n' "$result" | grep -qx 'expectedFailures:112'
  grep -qx 'expectedFailures:112' "$tmp/test_summary.log"
done
unset TESTS
result=$(zopen_check_results "$tmp" test)
printf '%s\n' "$result" | grep -qx 'expectedFailures:112'

# An empty log is a runner failure, never a successful zero-test run.
: > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
for expected in actualPasses:0 actualSkips:0 actualFailures:1 expectedFailures:0 totalTests:1 testRunIncomplete:1; do
  printf '%s\n' "$result" | grep -qx "$expected"
  grep -qx "$expected" "$tmp/test_summary.log"
done

# Match run-tests.php progress output: TEST prefix, CR, ANSI-colored status.
# A crash in php_pcre2_code_free leaves no final summary or last test result.
{
  printf 'TEST 11534/15000 [ext/pcre/tests/pass.phpt]\r\033[1;32mPASS\033[0m pass [ext/pcre/tests/pass.phpt]\n'
  printf 'TEST 11535/15000 [ext/pcre/tests/skip.phpt]\r\033[1;33mSKIP\033[0m skip [ext/pcre/tests/skip.phpt]\n'
  printf 'TEST 11536/15000 [ext/pcre/tests/fail.phpt]\r\033[1;31mFAIL\033[0m fail [ext/pcre/tests/fail.phpt]\n'
  printf 'TEST 11537/15000 [ext/pcre/tests/bork.phpt]\r\033[1;31mBORK\033[0m bork [ext/pcre/tests/bork.phpt]\n'
  printf 'TEST 11538/15000 [ext/pcre/tests/warn.phpt]\r\033[1;33mWARN\033[0m warn [ext/pcre/tests/warn.phpt]\n'
  printf 'TEST 11539/15000 [ext/pcre/tests/test11539.phpt]\rCEE3204S The system detected a protection exception in php_pcre2_code_free.\n'
} > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
for expected in actualPasses:1 actualSkips:1 actualFailures:4 expectedFailures:0 totalTests:5 testRunIncomplete:1 partialPasses:1 partialSkips:1 partialFailures:1 partialBorks:1 partialWarnings:1; do
  printf '%s\n' "$result" | grep -qx "$expected"
  grep -qx "$expected" "$tmp/test_summary.log"
done

# Partial failures count in addition to the runner failure, without a summary.
printf '%s\n' 'PASS one [tests/one.phpt]' 'FAIL two [tests/two.phpt]' > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
for expected in actualPasses:1 actualFailures:2 expectedFailures:0 totalTests:3 testRunIncomplete:1 partialFailures:1; do
  printf '%s\n' "$result" | grep -qx "$expected"
  grep -qx "$expected" "$tmp/test_summary.log"
done

# Many passes cannot hide a runner crash behind the full-suite allowance.
# Even a single final summary counter is insufficient to declare completion.
for final_counter in '' 'Tests passed    : 999 (100.0%)' 'Tests failed    : 0 (0.0%)'; do
  : > "$tmp/test_check.log"
  n=0
  while [ "$n" -lt 120 ]; do
    printf '%s\n' 'PASS completed [tests/completed.phpt]' >> "$tmp/test_check.log"
    n=$((n+1))
  done
  printf '%s\n' "$final_counter" >> "$tmp/test_check.log"
  result=$(zopen_check_results "$tmp" test)
  for expected in actualPasses:120 actualFailures:1 expectedFailures:0 totalTests:121 testRunIncomplete:1; do
    printf '%s\n' "$result" | grep -qx "$expected"
    grep -qx "$expected" "$tmp/test_summary.log"
  done
done

# A summary is not proof of clean shutdown: CI can crash after reporting it.
TESTS=ext
for make_name in make gmake 'make[1]'; do
  printf '%s\n' 'Tests skipped : 0' 'Tests failed : 0' 'Tests passed : 8' \
    "$make_name: *** [Makefile:204: test] Error 139" > "$tmp/test_check.log"
  result=$(zopen_check_results "$tmp" test)
  for expected in actualPasses:8 actualFailures:1 expectedFailures:0 totalTests:9 testRunIncomplete:1; do
    printf '%s\n' "$result" | grep -qx "$expected"
  done
done
# Normal make Error 1 from test failures is still governed by the baseline.
printf '%s\n' 'Tests failed : 1' 'Tests passed : 8' \
  'make: *** [Makefile:204: test] Error 1' > "$tmp/test_check.log"
result=$(zopen_check_results "$tmp" test)
printf '%s\n' "$result" | grep -qx 'actualFailures:1'
printf '%s\n' "$result" | grep -qx 'expectedFailures:112'
printf '%s\n' "$result" | grep -qx 'testRunIncomplete:0'

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
# The normal build must include PDO_ODBC and discover the private CI SDK
# without a job-specific flag. These are fixtures, not IBM SDK contents.
(
  unset ZOPEN_PDO_ODBC PHP_DB2_SDK_ROOT PDO_ODBC_DB2_INCLUDE PDO_ODBC_DB2_SIDEDECK
  ZOPEN_ROOTFS="$tmp/root"
  sdk="$ZOPEN_ROOTFS/etc/php-db2-sdk"
  mkdir -p "$sdk/include" "$sdk/lib" "$tmp/source/main"
  printf '%s\n' 'SDK header fixture' > "$sdk/include/sqlcli1.h"
  printf '%s\n' 'SDK side-deck fixture' > "$sdk/lib/dsnao64c.x"
  . "$port/buildenv"
  [ "$ZOPEN_PDO_ODBC" = static ]
  [ "$PDO_ODBC_DB2_INCLUDE" = "$sdk/include" ]
  [ "$PDO_ODBC_DB2_SIDEDECK" = "$sdk/lib/dsnao64c.x" ]
  zopen_post_buildenv
  case "$ZOPEN_EXTRA_CONFIGURE_OPTS" in
    *--with-pdo-odbc=ibm-db2-zos*) ;;
    *) echo 'Default build does not enable built-in PDO_ODBC' >&2; exit 1 ;;
  esac
  printf '%s\n' '#define PDO_ODBC_DB2_ZOS 1' > "$tmp/source/main/php_config.h"
  printf '%s\n' "CFLAGS=-I$PDO_ODBC_DB2_INCLUDE" "LIBS=$PDO_ODBC_DB2_SIDEDECK" > "$tmp/source/Makefile"
  (cd "$tmp/source"; zopen_pre_build)
  for bad_mode in no shared; do
    if (cd "$tmp/source"; ZOPEN_PDO_ODBC=$bad_mode; zopen_pre_build) 2>/dev/null; then
      echo 'Stale cached build mode was accepted' >&2; exit 1
    fi
  done
  if (cd "$tmp/source"; PDO_ODBC_DB2_SIDEDECK="$sdk/other.x"; zopen_pre_build) 2>/dev/null; then
    echo 'Stale cached SDK paths were accepted' >&2; exit 1
  fi
  if (PDO_ODBC_DB2_INCLUDE="$tmp/missing"; zopen_post_buildenv) 2>/dev/null; then
    echo 'Missing native SDK was accepted' >&2; exit 1
  fi
  # Explicit caller paths must win over the site SDK discovery.
  PDO_ODBC_DB2_INCLUDE="$tmp/explicit/include"
  PDO_ODBC_DB2_SIDEDECK="$tmp/explicit/native.x"
  . "$port/buildenv"
  [ "$PDO_ODBC_DB2_INCLUDE" = "$tmp/explicit/include" ]
  [ "$PDO_ODBC_DB2_SIDEDECK" = "$tmp/explicit/native.x" ]
)
printf '%s\n' 'buildenv checks passed'
