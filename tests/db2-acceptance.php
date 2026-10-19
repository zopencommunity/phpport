<?php
/** Live native Db2 acceptance. Run only through db2-acceptance.sh. */
declare(strict_types=1);

final class Db2AcceptanceRefused extends RuntimeException {}
final class Db2AcceptanceAssertion extends RuntimeException {}
$report = ['ok' => false, 'version' => PHP_VERSION, 'sapi' => PHP_SAPI, 'checks' => []];
$stage = 'configuration';
$created = false;
$db = $observer = null;
$manifest = null;
$exit = 1;

function requireTest(bool $ok, string $label): void {
    if (!$ok) throw new Db2AcceptanceAssertion($label);
}
function refuseUnless(bool $ok, string $label): void {
    if (!$ok) throw new Db2AcceptanceRefused($label);
}
function errorSummary(Throwable $e): array {
    $result = ['type' => $e instanceof PDOException ? 'pdo' : 'assertion'];
    if ($e instanceof PDOException) {
        $state = (string)$e->getCode();
        $result['sqlstate'] = preg_match('/^[A-Z0-9]{5}$/D', $state) ? $state : 'unknown';
        $native = $e->errorInfo[1] ?? null;
        if (is_int($native) || (is_string($native) && preg_match('/^-?[0-9]+$/D', $native))) {
            $result['native_code'] = (int)$native;
        }
    }
    if ($e instanceof Db2AcceptanceAssertion) $result['assertion'] = $e->getMessage();
    // Only our static assertion labels, never vendor messages/SQL/credentials.
    return $result;
}
function writeObjectManifest(string $path, array $record, bool $initial): void {
    $temporary = $initial ? $path : $path . '.' . bin2hex(random_bytes(4)) . '.new';
    $stream = fopen($temporary, 'xb');
    requireTest(is_resource($stream), 'cannot create private object manifest');
    try {
        requireTest(chmod($temporary, 0600), 'cannot protect object manifest');
        $payload = json_encode($record, JSON_THROW_ON_ERROR);
        for ($offset = 0; $offset < strlen($payload); $offset += $written) {
            $written = fwrite($stream, substr($payload, $offset));
            requireTest(is_int($written) && $written > 0, 'incomplete object manifest write');
        }
        requireTest(fflush($stream), 'cannot flush object manifest');
        if (function_exists('fsync')) requireTest(fsync($stream), 'cannot sync object manifest');
    } finally { fclose($stream); }
    if (!$initial) requireTest(rename($temporary, $path), 'cannot atomically update object manifest');
}
function scalar(PDO $db, string $sql, array $parameters = []): mixed {
    $stmt = $db->prepare($sql);
    $stmt->execute($parameters);
    try { return $stmt->fetchColumn(); }
    finally { $stmt->closeCursor(); }
}
function insertRow(PDO $db, string $table, int $id, ?string $text, mixed $blob, mixed $clob): void {
    $stmt = $db->prepare("INSERT INTO $table (ID, TXT, BIN, CHARS) VALUES (?, ?, ?, ?)");
    $stmt->bindValue(1, $id, PDO::PARAM_INT);
    $stmt->bindValue(2, $text, $text === null ? PDO::PARAM_NULL : PDO::PARAM_STR);
    $stmt->bindParam(3, $blob, $blob === null ? PDO::PARAM_NULL : PDO::PARAM_LOB);
    $stmt->bindParam(4, $clob, $clob === null ? PDO::PARAM_NULL : PDO::PARAM_LOB);
    try { $stmt->execute(); }
    finally { $stmt->closeCursor(); }
}
function fetchRow(PDO $db, string $table, int $id): array {
    $stmt = $db->prepare("SELECT TXT, BIN, CHARS FROM $table WHERE ID = ?");
    $stmt->execute([$id]);
    try {
        $row = $stmt->fetch(PDO::FETCH_ASSOC);
        requireTest(is_array($row), 'missing row');
        return $row;
    } finally { $stmt->closeCursor(); }
}
function remainingStream(string $payload): mixed {
    $s = fopen('php://temp', 'w+b');
    requireTest(is_resource($s), 'stream creation failed');
    fwrite($s, 'discarded-prefix' . $payload);
    fseek($s, strlen('discarded-prefix'));
    return $s;
}
// Deliberately provides no stat or seek implementation: input length is unknown.
final class Db2UnknownLengthStream {
    public $context;
    public static string $payload = '';
    private int $position = 0;
    public function stream_open(string $path, string $mode, int $options, ?string &$opened): bool { return true; }
    public function stream_read(int $count): string {
        $part = substr(self::$payload, $this->position, $count);
        $this->position += strlen($part);
        return $part;
    }
    public function stream_eof(): bool { return $this->position >= strlen(self::$payload); }
    public function stream_stat(): false { return false; }
}

try {
    // Authenticate a CGI invocation before inspecting any private DB settings.
    if (PHP_SAPI !== 'cli') {
        $token = getenv('PHP_DB2_ACCEPTANCE_TOKEN');
        $request = $_SERVER['HTTP_X_PHP_DB2_ACCEPTANCE_TOKEN'] ?? '';
        refuseUnless(PHP_SAPI === 'cgi-fcgi' && is_string($token) && strlen($token) >= 32
            && hash_equals($token, $request) && ($_SERVER['REMOTE_ADDR'] ?? '') === '127.0.0.1', 'private-cgi-runner-required');
    }
    $dsn = getenv('PDO_ODBC_TEST_DSN');
    $schema = getenv('PDO_ODBC_TEST_SCHEMA');
    $directory = getenv('PHP_DB2_TEST_REPORT_DIR');
    refuseUnless(getenv('PHP_DB2_TEST_ALLOW_WRITES') === 'yes', 'explicit-write-approval-required');
    refuseUnless(is_string($schema) && preg_match('/^[A-Z][A-Z0-9_]{0,127}$/D', $schema) === 1
        && !str_starts_with($schema, 'SYS')
        && !in_array($schema, ['PUBLIC', 'NULLID', 'SQLJ', 'SESSION', 'DB2GSE'], true), 'approved-disposable-schema-required');
    refuseUnless(is_string($dsn) && str_starts_with($dsn, 'odbc:') && strlen($dsn) > 5, 'private-odbc-dsn-required');
    refuseUnless(is_string($directory) && is_dir($directory) && is_writable($directory), 'private-report-directory-required');
    refuseUnless(PHP_INT_SIZE === 8 && PHP_OS === 'OS/390', 'native-64-bit-zos-required');
    refuseUnless(extension_loaded('pdo_odbc') && in_array('odbc', PDO::getAvailableDrivers(), true), 'built-in-pdo-odbc-required');
    $ini = getenv('DSNAOINI');
    refuseUnless(is_string($ini) && is_file($ini) && is_readable($ini), 'approved-readable-uss-dsnaoini-required');
    $username = getenv('PDO_ODBC_TEST_USER');
    $password = getenv('PDO_ODBC_TEST_PASS');
    $connect = static fn(array $options = []) => new PDO($dsn,
        $username === false ? null : $username, $password === false ? null : $password,
        $options + [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_PERSISTENT => false]);
    $name = 'ZP8_' . strtoupper(bin2hex(random_bytes(6)));
    $table = '"' . $schema . '"."' . $name . '"';
    $manifest = $directory . '/objects-' . $name . '.json';
    $record = ['schema' => $schema, 'table' => $name, 'created' => false, 'sapi' => PHP_SAPI];
    writeObjectManifest($manifest, $record, true);

    $stage = 'connect'; $db = $connect();
    requireTest($db->getAttribute(PDO::ATTR_CLIENT_VERSION) === 'ODBC-ibm-db2-zos', 'native Db2 driver flavor required');
    $report['checks'][] = $stage;
    $stage = 'unsafe-persistence-rejected';
    $rejected = false;
    try { $unexpected = $connect([PDO::ATTR_PERSISTENT => true]); $unexpected = null; }
    catch (PDOException $e) { $rejected = $e->getCode() === 'IM001'; $e = null; }
    requireTest($rejected, 'unsafe native persistence was not rejected');
    $report['checks'][] = $stage;
    $stage = 'prepared-integer';
    requireTest((int)scalar($db, 'SELECT CAST(? AS INTEGER) FROM SYSIBM.SYSDUMMY1', [42]) === 42, 'prepared integer mismatch');
    $report['checks'][] = $stage;

    $stage = 'create-owned-unicode-lob-table';
    $db->exec("CREATE TABLE $table (ID INTEGER NOT NULL PRIMARY KEY, TXT VARCHAR(2048), BIN BLOB(1048576), CHARS CLOB(1048576)) CCSID UNICODE");
    $created = true; $record['created'] = true;
    writeObjectManifest($manifest, $record, false);
    $report['checks'][] = $stage;

    $text = "PHP 'quoted' \\ text\n" . "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x99\x82";
    $clob = str_repeat($text . "\n", 1000);
    $blob = str_repeat(pack('C*', ...range(0, 255)), 1024);
    $stage = 'prepared-unicode-and-multiple-lobs';
    insertRow($db, $table, 1, $text, $blob, $clob);
    $row = fetchRow($db, $table, 1);
    requireTest($row['TXT'] === $text && $row['BIN'] === $blob && $row['CHARS'] === $clob, 'text or LOB mismatch');
    requireTest(strtoupper((string)scalar($db, "SELECT HEX(TXT) FROM $table WHERE ID = ?", [1])) === strtoupper(bin2hex($text)), 'server Unicode bytes mismatch');
    $report['checks'][] = $stage;

    $stage = 'null-and-empty-lobs';
    insertRow($db, $table, 2, null, null, null);
    insertRow($db, $table, 3, '', '', '');
    requireTest(fetchRow($db, $table, 2) === ['TXT' => null, 'BIN' => null, 'CHARS' => null], 'NULL mismatch');
    requireTest(fetchRow($db, $table, 3) === ['TXT' => '', 'BIN' => '', 'CHARS' => ''], 'empty versus NULL mismatch');
    $report['checks'][] = $stage;

    $stage = 'stream-remaining-bytes-and-buffer-boundaries';
    foreach ([0, 1, 8191, 8192, 8193, 65537, 262144] as $i => $length) {
        $payload = substr($blob, 0, $length);
        $stream = remainingStream($payload);
        try { insertRow($db, $table, 100 + $i, 'stream', $stream, 'stream-text'); }
        finally { fclose($stream); }
        requireTest(fetchRow($db, $table, 100 + $i)['BIN'] === $payload, 'stream payload mismatch');
    }
    $report['checks'][] = $stage;

    $stage = 'unknown-length-input-stream';
    requireTest(stream_wrapper_register('db2accept', Db2UnknownLengthStream::class), 'stream wrapper registration failed');
    Db2UnknownLengthStream::$payload = $blob;
    $stream = fopen('db2accept://input', 'rb');
    requireTest(is_resource($stream), 'unknown-length stream creation failed');
    try { insertRow($db, $table, 200, 'unknown-size', $stream, $clob); }
    finally { fclose($stream); stream_wrapper_unregister('db2accept'); }
    requireTest(fetchRow($db, $table, 200)['BIN'] === $blob, 'unknown stream mismatch');
    $stream = remainingStream($clob);
    try { insertRow($db, $table, 201, 'clob-stream', $blob, $stream); }
    finally { fclose($stream); }
    requireTest(fetchRow($db, $table, 201)['CHARS'] === $clob, 'remaining CLOB stream mismatch');
    requireTest(stream_wrapper_register('db2accept', Db2UnknownLengthStream::class), 'CLOB wrapper registration failed');
    Db2UnknownLengthStream::$payload = $clob;
    $stream = fopen('db2accept://clob', 'rb');
    requireTest(is_resource($stream), 'unknown CLOB stream creation failed');
    try { insertRow($db, $table, 202, 'unknown-clob', $blob, $stream); }
    finally { fclose($stream); stream_wrapper_unregister('db2accept'); }
    requireTest(fetchRow($db, $table, 202)['CHARS'] === $clob, 'unknown CLOB stream mismatch');
    $report['checks'][] = $stage;

    $stage = 'separate-connections-commit-and-rollback';
    $observer = $connect();
    $db->beginTransaction(); insertRow($db, $table, 300, 'commit', '', ''); $db->commit();
    requireTest((int)scalar($observer, "SELECT COUNT(*) FROM $table WHERE ID = ?", [300]) === 1, 'commit not visible');
    $db->beginTransaction(); insertRow($db, $table, 301, 'rollback', '', ''); $db->rollBack();
    requireTest((int)scalar($observer, "SELECT COUNT(*) FROM $table WHERE ID = ?", [301]) === 0, 'rollback failed');
    insertRow($db, $table, 302, 'restored-autocommit', '', '');
    $db = null;
    requireTest((int)scalar($observer, "SELECT COUNT(*) FROM $table WHERE ID = ?", [302]) === 1, 'autocommit not restored');
    $report['checks'][] = $stage;

    $stage = 'connection-destruction-rolls-back';
    $transient = $connect(); $transient->beginTransaction();
    insertRow($transient, $table, 303, 'uncommitted', '', ''); $transient = null;
    requireTest((int)scalar($observer, "SELECT COUNT(*) FROM $table WHERE ID = ?", [303]) === 0, 'destruction rollback failed');
    $transient = $connect([PDO::ATTR_AUTOCOMMIT => false]);
    insertRow($transient, $table, 304, 'implicit-uncommitted', '', ''); $transient = null;
    requireTest((int)scalar($observer, "SELECT COUNT(*) FROM $table WHERE ID = ?", [304]) === 0, 'implicit transaction leaked');
    $report['checks'][] = $stage;
    $exit = 0;
} catch (Db2AcceptanceRefused $e) {
    $report['refused'] = $e->getMessage(); $exit = 77; $e = null;
} catch (Throwable $e) {
    $report['failure'] = ['stage' => $stage] + errorSummary($e); $e = null;
} finally {
    // Release all request-owned handles before DROP, including implicit work.
    $db = $transient = null;
    if ($created) {
        try {
            $cleaner = $observer ?? $connect();
            $cleaner->exec("DROP TABLE $table");
            $report['cleanup'] = 'owned-table-dropped';
            if ($manifest !== null) unlink($manifest);
        } catch (Throwable $e) {
            $report['cleanup'] = 'failed-see-private-object-manifest';
            $report['cleanup_error'] = errorSummary($e); $e = null; $exit = 1;
        }
    } elseif ($manifest !== null) {
        // Keep the private planned-object record if CREATE failed ambiguously.
        $report['cleanup'] = 'no-confirmed-created-table';
    }
    $observer = $cleaner = null;
}
$report['ok'] = $exit === 0;
if (PHP_SAPI !== 'cli') {
    http_response_code($exit === 0 ? 200 : 503);
    header('Content-Type: application/json');
}
echo json_encode($report, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES), "\n";
exit($exit);
