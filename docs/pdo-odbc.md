# Native Db2 PDO_ODBC on z/OS (experimental)

The standard build leaves PDO_ODBC **disabled** (`ZOPEN_PDO_ODBC=no`).
The opt-in shared implementation builds, installs and loads with PHP
8.2.13 and Clang 21, but connection, LOB and transaction validation is
**outstanding**. Loading the driver is not proof of working database access.

## Build modes and prerequisites

| `ZOPEN_PDO_ODBC` | Behaviour |
| --- | --- |
| `no` (default) | No native Db2 PDO_ODBC support; standard build unchanged. |
| `shared` | Built-in PDO plus a shared PDO_ODBC module; disables CGI and phpdbg. |
| `static` | Opt-in built-in PDO_ODBC; pending separate validation. |

The native Db2 SDK is site-supplied proprietary IBM software, **not a zopen
package dependency**. Obtain it through your Db2 administrator; do not commit
or redistribute IBM headers, side decks, credentials or site configuration.
This path uses native Db2 for z/OS ODBC, not the distributed LUW `clidriver`.

Configure requires both environment variables for either enabled mode:

- `PDO_ODBC_DB2_INCLUDE`: absolute USS directory containing the native Db2
  headers, including `sqlcli1.h` and its related headers.
- `PDO_ODBC_DB2_SIDEDECK`: absolute USS filename of the 64-bit native
  `DSNAO64C` import side deck (`.x`), not a load module or directory.

Use the [python-ibmdb native z/OS installation reference][python-install] and
its [build/extraction code][python-build] as preparation references: extract
headers from the site's `SDSNC.H` data set, preserving the header layout and
include relationships. Export `DSNAO64C` from `SDSNMACS` using TSO `OPUT` to a
USS `.x` file, preserving its records and usable encoding. Use your site's
actual data set names and IBM procedures. Do not copy the Python binary module
(or another PHP binary module); these references describe SDK/runtime setup,
not PHP binary compatibility.

Example (all paths are placeholders; run from this repository):

```sh
export ZOPEN_PDO_ODBC=shared
export PDO_ODBC_DB2_INCLUDE=/absolute/path/to/native-db2/include
export PDO_ODBC_DB2_SIDEDECK=/absolute/path/to/native-db2/DSNAO64C.x
zopen-build -v
```

Run `zopen-build` in the foreground. Use `ZOPEN_PDO_ODBC=static` only for
separate static-mode validation; unset the variable or set it to `no` for the
standard build. When changing modes or SDK paths in an existing checkout,
use a clean build (`zopen-build -v -f`); cached configuration otherwise retains
the previous options.

## Deliver and load the matching pair

Shared mode uses native linking against the **matching PHP CLI import deck**.
The module must be built and delivered **with its matching PHP core**. It is
not an add-on that can be loaded into an old stock PHP installation, even if
the PHP version string matches. Keep the CLI and module from the same build.
CGI/phpdbg are disabled in this mode; their compatibility is not established.

At runtime:

- Put the matching PHP installation's `bin` directory in `LIBPATH` so the
  module can resolve its PHP executable imports. For an uninstalled build,
  this is the matching source tree's `sapi/cli` directory.
- Set `STEPLIB` to the site's required Db2 load libraries (typically
  `SDSNEXIT`, `SDSNLOAD`, `SDSNLOD2`), using administrator-approved qualifiers
  and ordering. `LIBPATH` does not replace `STEPLIB`.
- Set `DSNAOINI` to the site's native ODBC initialization file.

A load-only check, using absolute paths to artifacts from the same build:

```sh
export LIBPATH="/absolute/path/to/matching-php/bin${LIBPATH:+:${LIBPATH}}"
/absolute/path/to/matching-php/bin/php -n \
  -d extension=/absolute/path/to/matching-module/pdo_odbc.so --ri pdo_odbc
```

Static mode does not load a separate extension; it still needs the Db2 runtime
configuration. Do not infer static validation from the shared-mode load proof.

## DSNAOINI: bytes matter

Follow the [IBM Db2 ODBC Guide][ibm-odbc] and coordinate subsystem, plan,
authorization and attachment settings with your Db2 administrator. The
python-ibmdb reference uses `CURRENTAPPENSCH=ASCII`, `FLOAT=IEEE` and
`MULTICONTEXT=2` in `[COMMON]`, with `MVSATTACHTYPE=RRSAF` in the subsystem
stanza. These are Python-reference settings, **not universally verified PHP
requirements** or a substitute for site configuration.

The initialization file must contain **EBCDIC IBM-1047 bytes**. A tag alone
never converts ASCII/UTF-8 contents. For example, convert a separate input
file whose actual encoding is ISO8859-1:

```sh
iconv -f ISO8859-1 -t IBM-1047 /absolute/path/to/odbc.ini.input \
  > /absolute/path/to/odbc.ini.ebcdic
chtag -b /absolute/path/to/odbc.ini.ebcdic
export DSNAOINI=/absolute/path/to/odbc.ini.ebcdic
chtag -p "$DSNAOINI"
```

Use `-f UTF-8` instead if the input is UTF-8. Ensure the output contains the
converted bytes, without an additional automatic conversion. The reference
requires **T=off**: binary tagging as above, or mixed IBM-1047 tagging
(`chtag -m -c IBM-1047 "$DSNAOINI"`). Do not use a text tag that triggers
conversion; the Python reference warns of S0C4 abends with text tagging.

## Tests, limitations and diagnosis

Optional targeted tests (same SDK/runtime environment as above):

```sh
TESTS=ext/pdo_odbc NO_INTERACTION=1 REPORT_EXIT_STATUS=1 zopen-build -v
```

The focused suite currently has 8 passes, 85 skips and no failures without
a database DSN; skips are not counted as passes. `sh tests/buildenv.sh` checks
the opt-in configuration and test-summary accounting without requiring Db2.

Most database tests skip without the test suite's environment-provided DSN
and connection settings (`PDO_ODBC_TEST_DSN`, `PDO_ODBC_TEST_USER`, and
`PDO_ODBC_TEST_PASS`). **Use only a disposable test database/schema**: the
upstream PDO tests create and modify tables. Supply settings privately; never
commit credentials, live site qualifiers or sensitive logs. Skips and a
successful load do not validate connections, LOB round trips or
commit/rollback; those checks remain outstanding.

Native-path constraints include disabled ODBC driver-manager connection
pooling, forward fetching via `SQLFetch` for LOB handling, and the native
**32-bit `SQLLEN` ABI even in a 64-bit process**. Do not assume unixODBC ABI,
scrollable LOB cursors or pooling behaviour.

- Loader/import errors: check that PHP and module are a matching build and
  that `LIBPATH` and Db2 `STEPLIB` are correct.
- SQLCODE `-805`: ask the DBA to check native ODBC package/plan binds and the
  site's customized `SDSNSAMP(DSNTIJCL)` job, plus execution authorization.
- For native diagnostics, use IBM's `APPLTRACE`/`DIAGTRACE` guidance. Protect
  trace files, which can contain SQL, data and connection information.
- [IBM APAR PH41045][ph41045] concerns minimum maintenance cited for IBM Python
  support. Have the DBA assess applicability; it is **not an unconditional,
  PHP-verified prerequisite**.

## References

- [IBM Db2 for z/OS ODBC Guide and Reference][ibm-odbc]
- [IBM Python support for Db2 for z/OS](https://www.ibm.com/docs/en/db2-for-zos/12.0.0?topic=zos-python)
- [IBM ODBC troubleshooting, binding, tagging and tracing](https://www.ibm.com/docs/api/v1/content/SSEPEK_12.0.0/odbc/src/tpc/db2z_cd4odbcpython.html?lang=en)
- [IBM APAR PH41045][ph41045]
- [python-ibmdb](https://github.com/ibmdb/python-ibmdb), especially its
  [native z/OS installation instructions][python-install].

[ibm-odbc]: https://www.ibm.com/support/knowledgecenter/SSEPEK/pdf/db2z_12_odbcbook.pdf
[ph41045]: https://www.ibm.com/support/pages/apar/PH41045
[python-install]: https://github.com/ibmdb/python-ibmdb/blob/master/INSTALL.md#inszos
[python-build]: https://github.com/ibmdb/python-ibmdb/blob/master/setup.py
