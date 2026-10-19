<?php
// Unit-test trusted harness helpers only. The live main body is never evaluated.
declare(strict_types=1);
$source = file_get_contents(__DIR__ . '/db2-acceptance.php');
$end = strpos($source, "\ntry {\n    // Authenticate");
if ($end === false) throw new RuntimeException('Cannot isolate DB-free harness prelude');
eval(substr($source, 5, $end - 5));
$directory = $argv[1];
$path = $directory . '/manifest.json';
$record = ['schema' => 'ZOPEN_TEST', 'table' => 'ZP8_FIXTURE', 'created' => false];
writeObjectManifest($path, $record, true);
requireTest(json_decode(file_get_contents($path), true) === $record, 'initial manifest incomplete');
$record['created'] = true;
writeObjectManifest($path, $record, false);
requireTest(json_decode(file_get_contents($path), true) === $record, 'updated manifest incomplete');
requireTest((fileperms($path) & 0777) === 0600, 'manifest permissions');

final class Db2ManifestFailStream {
    public $context;
    public static array $files = ['manifestfail://existing' => 'original-record'];
    public static int $renames = 0;
    private string $path;
    private bool $wrote = false;
    public function stream_open($path, $mode, $options, &$opened): bool {
        if (isset(self::$files[$path])) return false;
        $this->path = $path; self::$files[$path] = ''; return true;
    }
    public function stream_metadata($path, $option, $value): bool { return true; }
    public function stream_write(string $data): int {
        if ($this->wrote) return 0;
        $this->wrote = true; $part = substr($data, 0, 3);
        self::$files[$this->path] .= $part; return strlen($part);
    }
    public function stream_flush(): bool { return true; }
    public function stream_close(): void {}
    public function rename($from, $to): bool { self::$renames++; return false; }
}
stream_wrapper_register('manifestfail', Db2ManifestFailStream::class);
set_error_handler(static fn() => true); // Expected simulated write failures only.
try {
    foreach ([true, false] as $initial) {
        $failed = false;
        try { writeObjectManifest($initial ? 'manifestfail://new' : 'manifestfail://existing', $record, $initial); }
        catch (Db2AcceptanceAssertion $e) { $failed = true; }
        requireTest($failed, 'partial write was not rejected');
    }
} finally { restore_error_handler(); stream_wrapper_unregister('manifestfail'); }
requireTest(Db2ManifestFailStream::$files['manifestfail://existing'] === 'original-record'
    && Db2ManifestFailStream::$renames === 0, 'partial update replaced existing recovery record');
echo "Object manifest durability checks passed; no database operations performed.\n";
