#!/bin/sh
# Standalone or appended after trusted PHPPORT_INSTALL_MODE/VERSION assignments.
set +x
set -eu
umask 077
fail() { printf '%s\n' 'FAIL: installed PHP validation' >&2; exit 1; }
case $# in
  0) prefix=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || fail
     mode=${PHPPORT_INSTALL_MODE:-}; version=${PHPPORT_INSTALL_VERSION:-} ;;
  3) prefix=$1; mode=$2; version=$3 ;;
  *) fail ;;
esac
case $prefix in /*) ;; *) fail ;; esac
case $mode in no|static|shared) ;; *) fail ;; esac
case $version in ''|*[!0-9.]*) fail ;; esac
[ -x "$prefix/bin/php" ] || fail
if [ "$mode" = static ]; then
  [ -x "$prefix/bin/php-cgi" ] && [ -x "$prefix/bin/phpdbg" ] || fail
fi
command -v timeout >/dev/null 2>&1 || fail
tmp=$(mktemp -d "${TMPDIR:-/tmp}/phpport-install.XXXXXX") || fail
passed=no
cleanup() {
  if [ "$passed" = yes ]; then rm -rf -- "$tmp"
  else
    rm -f -- "$tmp/probe.php" "$tmp/modules"
    printf 'Private validation logs: %s\n' "$tmp" >&2
  fi
}
trap cleanup 0
trap 'fail' 1 2 3 15
: > "$tmp/run.log"
: > "$tmp/modules"
: > "$tmp/probe.php"
if [ "$(uname -s)" = OS/390 ]; then
  chtag -tc ISO8859-1 "$tmp/run.log" "$tmp/modules" "$tmp/probe.php" \
    > /dev/null 2>&1 || fail
fi
# Discover before executing PHP: never guess a build-tree or another CLI's DLL.
module=
if [ "$mode" = shared ]; then
  timeout -k 2 15 find "$prefix" -type f -name pdo_odbc.so \
    > "$tmp/modules" 2> "$tmp/run.log" || fail
  count=0
  while IFS= read -r candidate; do
    count=$((count + 1)); module=$candidate
  done < "$tmp/modules"
  [ "$count" = 1 ] && [ -r "$module" ] || fail
  LIBPATH="$prefix/bin${LIBPATH:+:$LIBPATH}"; export LIBPATH
fi
cat > "$tmp/probe.php" <<'PHPPORT_PROBE'
<?php
function check($ok) { if (!$ok) { throw new RuntimeException('install probe failed'); } }
check(PHP_VERSION === getenv('PHPPORT_PROBE_VERSION'));
check(PHP_INT_SIZE === 8 && PHP_OS === 'OS/390');
check(PHP_SAPI === getenv('PHPPORT_PROBE_SAPI'));
check(php_ini_loaded_file() === false && !php_ini_scanned_files());
$drivers = PDO::getAvailableDrivers();
check(in_array('sqlite', $drivers, true));
$odbc = getenv('PHPPORT_PROBE_MODE') !== 'no';
check(extension_loaded('pdo_odbc') === $odbc);
check(in_array('odbc', $drivers, true) === $odbc);
if ($odbc) {
    ob_start();
    (new ReflectionExtension('pdo_odbc'))->info();
    $info = ob_get_clean();
    check(str_contains($info, 'PDO Driver for ODBC (ibm-db2-zos)'));
}
// Only SQLite is instantiated: registration/reflection never opens Db2.
$db = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$db->exec('CREATE TABLE probe (b BLOB)');
$bytes = hex2bin('0001275c7f80feff00414243');
$stmt = $db->prepare('INSERT INTO probe VALUES (?)');
$stmt->bindValue(1, $bytes, PDO::PARAM_LOB);
check($stmt->execute());
check($db->query('SELECT b FROM probe')->fetchColumn() === $bytes);
for ($i = 0; $i < 16; ++$i) {
    check(preg_match('/^(ab)+([0-9]+)$/', 'abab123', $m) === 1 && $m[2] === '123');
}
$fiber = new Fiber(function () { return Fiber::suspend(7) + 1; });
check($fiber->start() === 7);
$fiber->resume(8);
check($fiber->isTerminated() && $fiber->getReturn() === 9);
$before = memory_get_usage();
$block = str_repeat('x', 1048576);
check(memory_get_usage() >= $before + 1048576);
unset($block);
check(memory_get_usage() < $before + 1048576);
echo 'PHPPORT_INSTALL_OK ', PHP_SAPI, "\n";
PHPPORT_PROBE
PHPPORT_PROBE_MODE=$mode; PHPPORT_PROBE_VERSION=$version
export PHPPORT_PROBE_MODE PHPPORT_PROBE_VERSION
# Run from our private directory so incidental dumps cannot touch the install.
cd -- "$tmp" || fail
run_probe() {
  PHPPORT_PROBE_SAPI=$1; export PHPPORT_PROBE_SAPI; shift
  timeout -k 5 30 "$@" > "$tmp/run.log" 2>&1 || fail
  tr -d '\r' < "$tmp/run.log" | grep -Fxq "PHPPORT_INSTALL_OK $PHPPORT_PROBE_SAPI" || fail
}
if [ "$mode" = shared ]; then
  run_probe cli "$prefix/bin/php" -n -d "extension=$module" "$tmp/probe.php"
else
  run_probe cli "$prefix/bin/php" -n "$tmp/probe.php"
fi
if [ "$mode" = static ]; then
  # Real CGI, not -f (which bypasses CGI request/header behavior).
  SCRIPT_FILENAME="$tmp/probe.php"; REQUEST_METHOD=GET; REDIRECT_STATUS=1
  GATEWAY_INTERFACE=CGI/1.1; SERVER_PROTOCOL=HTTP/1.1; SERVER_NAME=localhost
  QUERY_STRING=; CONTENT_LENGTH=0; SCRIPT_NAME=/probe.php; PATH_INFO=
  unset PATH_TRANSLATED PHP_VALUE PHP_ADMIN_VALUE || :
  export SCRIPT_FILENAME REQUEST_METHOD REDIRECT_STATUS GATEWAY_INTERFACE
  export SERVER_PROTOCOL SERVER_NAME QUERY_STRING CONTENT_LENGTH SCRIPT_NAME PATH_INFO
  run_probe cgi-fcgi "$prefix/bin/php-cgi" -n
  run_probe phpdbg "$prefix/bin/phpdbg" -n -qrr "$tmp/probe.php"
fi
passed=yes
printf '%s\n' 'PASS: installed PHP validation'
