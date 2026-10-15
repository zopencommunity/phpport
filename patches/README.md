# PHP z/OS patches

The existing per-file patches provide the original z/OS port adaptations.
`configure.patch` retains dependency link flags in the release configure script.

## `pdo_odbc-zos.patch` - issue #39

Adds the `ibm-db2-zos` PDO_ODBC flavour, using site-supplied native headers and
an explicit 64-bit `DSNAO64C` side deck. Enabled builds regenerate configure
with `buildconf --force`; the configure.ac adaptation preserves the existing
z/OS dependency-link workaround when doing so. No IBM SDK files are included.

Native shared mode uses a z/OS make fragment rather than libtool's unsupported
DLL path, exports the PHP CLI import deck, links the module against that core,
and installs the module. It requires built-in PDO and disables CGI/phpdbg;
the module must be distributed with its matching PHP executable. The patch
also uses native handle typedefs/null constants, disables unsupported ODBC
pooling, and uses SQLFetch for forward LOB fetching. A connection-free PHPT
checks driver registration. Static linkage is an experimental alternative.

## `compiler-compat.patch`

Build prerequisites exposed by PHP 8.2.13 with the current toolchain:

- Rename a timelib prototype parameter to avoid the z/OS `tzname` macro.
- Backport PHP-8.2 libxml error-handler constness and unused-parameter fixes
  for libxml2 >= 2.12 (tested with 2.15.4).

## Validation

On z/OS with Clang 21, PHP 8.2.13 builds and installs in shared mode. The
installed CLI starts without loading PDO_ODBC or requiring Db2; explicitly
loading the module registers `odbc` alongside `sqlite`. Native ODBC pooling
is reported as unsupported. The focused PDO_ODBC suite has 8 passes, 85 skips,
and 0 failures without a database test DSN. Skips are excluded from passing
test metadata. The complete PHP suite, static mode, actual Db2 connections,
LOB round trips and transaction semantics are not yet validated.

See [native PDO_ODBC setup](../docs/pdo-odbc.md) for prerequisites and limits.
