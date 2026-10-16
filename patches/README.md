# PHP z/OS patches

These patches are rebased onto PHP **8.2.34**. `configure.patch` retains the
port's dependency-link flags in the release configure script.

## Native runtime and the PCRE cleanup S0C4

- `zend_alloc.c.patch` allocates whole, zeroed Zend MM chunks with zoslib
  `__aligned_malloc` and releases them with `__aligned_free`. In-place resize
  is declined so Zend allocates/copies/frees complete chunks. Normal Zend MM
  accounting and `memory_limit` remain enabled. The included PHPT checks data
  integrity across large grow/shrink/free operations.
- `zend_fibers.c.patch` uses above-bar, aligned storage for native AMODE-64
  contexts, with room for LE's megabyte-aligned guard. It checks getcontext
  and makecontext errors and pairs native allocation/free. Other platforms
  retain their upstream stack implementations.
- The former `zend_mmap.h.patch` is removed. A calloc pointer plus sentinel
  cannot implement aligned mappings, partial unmaps or page protection.
  Invented `MAP_ANON=16` flags are also removed: native value 16 is __MAP_64,
  not anonymous storage.

The earlier PCRE cleanup protection exception did not occur in the full
8.2.34 validation run. This is observed mitigation, not a conclusive diagnosis
of the original corruption mechanism. `USE_ZEND_ALLOC=0` is not used as a fix.

## Other native fixes

- `file-type-checks.patch` uses `S_ISDIR`/`S_ISREG` in fileinfo and the CLI
  server; raw type-bit tests misclassify regular files on z/OS.
- `hrtime.c.patch` and `hrtime.h.patch` enable a real monotonic clock and
  verify initialization instead of advertising a timer that returns zero.
- `process-time.patch` passes explicit child environments to exec, including
  empty environments. Native basename lookup uses the child's PATH, not a
  fallback to the parent's PATH; explicit paths use execve. Native sleeps of
  at least one second use normalized nanosleep without restarting on EINTR.
  PHPTs cover these behaviors.
- `compiler-compat.patch` renames a timelib parameter to avoid the native
  `tzname` macro. PHP 8.2.34 already contains the libxml constness fixes that
  needed backporting to 8.2.13.
- The obsolete `network.c.patch` is removed: zoslib supplies timercmp before
  both uses; the old late redefinition is unnecessary.

## Fixture portability and reporting

`test-portability.patch` narrowly permits IBM LE diagnostic prefixes,
punctuation and native errno numbers while preserving error meaning and
return-value checks. Fixed loopback ports reserved by the CI TCP/IP profile
are replaced with allocated ephemeral ports, propagated to clients/children.
No tests are skipped to hide these differences, and site TCP/IP policy is
not changed. PHP 8.2.34's new UDP select waits are preserved.

The invalid-filename `fileinfo/bug68996.phpt` change is an ASCII Git binary
hunk, preserving its exact UTF-8 expected bytes across fresh z/OS checkouts
of this ISO8859-1 working-tree repository. To regenerate it, mark that path
`-diff` in the source repository's `.git/info/attributes` and use
`git diff --binary HEAD`; do not replace its Unicode assertions with loose
wildcards. Verify patch replay from a fresh Git checkout, not only from the
pre-commit working files.

The buildenv reporter normalizes ANSI/CR output and parses real summary
counters. Interrupted runs preserve partial outcomes, add a runner failure,
set `testRunIncomplete:1`, and force the allowance to zero. Outer make signal
exits are rejected even after a printed summary. Empty logs cannot report a
successful zero-test run. Complete full/mixed runs retain the existing **112**
allowance; focused PDO_ODBC runs allow **zero**. Shell regressions run before
the native suite, including SDK-free builds.

## Native Db2 PDO_ODBC - issue #39

`pdo_odbc-zos.patch` implements the native `ibm-db2-zos` flavour. This branch
defaults to built-in PDO_ODBC in CLI, CGI/FastCGI and phpdbg, with private SDK
discovery and explicit SDK-free opt-out. Static means built-in PHP code, not
redistribution of IBM's runtime. Advanced shared mode remains matching-CLI-only;
it rejects FPM, CGI/phpdbg and other incompatible SAPI selections.

Native handles/SQLLEN retain their actual 32-bit SDK types in LP64. Forward
LOB fetch uses SQLFetch. Native BLOB input/results use binary requests; standard
binary types retain PDO_ODBC's existing result contract. Native data-at-exec,
empty/error/nonstat/seeked streams, capacity versus indicators and narrowing
bounds are hardened. Pure helpers and extracted real driver/statement functions
are exercised by database-free C mocks; an actual-SDK probe verifies widths
without calling Db2.

Credential completion independently handles missing top-level UID/PWD keys
with case-insensitive brace-aware parsing. Native connections avoid manager
pooling/cursor-library calls. Unsupported cursor emulation, persistence and the
Windows-only wide UTF8-conversion flag reject explicitly before runtime access.
Autocommit state changes only after successful API calls (including success
with information). Metadata/output bounds are checked before string copies.
Ambiguous native result lengths fail rather than returning stale/padded data.

The live CLI/CGI harness requires explicit write approval, a protected schema
and private configuration. Recovery manifests are checked/flushed before any
DB action and atomically updated; logs/default cwd stay private. Refusal and
manifest-failure paths, including CGI token checks, are tested without Db2.
The Microsoft Access fixture is now Windows-only: it is not a native Db2 test.
No IBM SDK, credentials or site configuration are included. **Live Db2 data
acceptance remains outstanding; this candidate is not a production claim.**

## Validation

A foreground `zopen-build` on the CI host built and installed PHP 8.2.34 with
exit **0**, using the ordinary SDK-free configuration. Its full four-worker
suite recorded **11,469 passes, 7,059 skips and 96 failures**, with no S0C4
and `testRunIncomplete:0`, against the unchanged allowance of **112**.
Meeting that gate does not establish that all remaining failures are harmless.

A fresh Git checkout of native candidate `8b1a857` completed the default
build/install/package with **exit 0**, discovering the private SDK without
mode/path overrides. Its full four-worker run recorded **11,482 passes,
7,136 skips and 93 failures**, no BORK/S0C4 and `testRunIncomplete:0`, against
the **unchanged allowance of 112**. The focused native suite separately passed
**10 tests, 90 skips, zero failures/BORKs**, with zero allowance. The unsupported
Access fixture is correctly limited to Windows; no Db2 fixture is skipped to
hide a failure.

CLI, a controlled CGI invocation and phpdbg all register `odbc` and `sqlite`.
The SDK probe confirms 8-byte pointers/SQLPOINTER and 4-byte handles/lengths.
These are build/protocol/availability results, not live data acceptance.

See [native PDO_ODBC setup and acceptance](../docs/pdo-odbc.md) for limits.
