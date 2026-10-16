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

`pdo_odbc-zos.patch` retains the opt-in `ibm-db2-zos` flavour using private,
site-supplied headers and the 64-bit DSNAO64C side deck. Native handles and
SQLLEN widths are preserved, driver-manager pooling is disabled, and forward
LOB fetching uses SQLFetch. Shared mode imports the matching CLI core; it is
not interchangeable with CGI/phpdbg or an unrelated PHP installation.

This CI hotfix **does not enable PDO_ODBC by default** or claim Db2 production
acceptance. Built-in multi-SAPI delivery remains a separate candidate pending
real connections, prepared statements, LOBs and transactions. No IBM SDK,
credentials or site configuration are included.

## Validation

A foreground `zopen-build` on the CI host built and installed PHP 8.2.34 with
exit **0**, using the ordinary SDK-free configuration. Its full four-worker
suite recorded **11,469 passes, 7,059 skips and 96 failures**, with no S0C4
and `testRunIncomplete:0`, against the unchanged allowance of **112**.
Meeting that gate does not establish that all remaining failures are harmless.

The older opt-in shared PDO_ODBC load proof was for PHP 8.2.13 (8 passes,
85 skips, no failures without a DSN); it does not establish 8.2.34 database
functionality. See [native PDO_ODBC setup](../docs/pdo-odbc.md) for limits.
