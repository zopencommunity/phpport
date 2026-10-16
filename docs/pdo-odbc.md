# Native Db2 PDO_ODBC on z/OS

## Status and delivery model

This PHP **8.2.34 candidate** builds PDO_ODBC into CLI, the CGI/FastCGI SAPI and
phpdbg by default. Applications use ordinary `new PDO('odbc:...', ...)`; no
`extension=pdo_odbc.so`, matching-CLI import deck or PHP rebuild is needed at
installation time. Remove any obsolete shared-extension setting when using a
built-in package.

**Not yet accepted against live Db2.** The public STABLE_phpport_462 release
has only SQLite; it is not this native candidate. A successful compile,
registration check or database-free test does not prove working connections,
Unicode data, LOBs or transactions. Issue #39 must remain open until the live
CLI and web acceptance checks pass with an approved disposable schema. FPM
and Apache module builds are not validated; CGI/FastCGI web delivery is the
tested SAPI path, not proof of FPM support.

"Static" below means built into each PHP executable. It does **not** mean
bundling or statically linking IBM's proprietary Db2 runtime.

## Build modes and private SDK

| `ZOPEN_PDO_ODBC` | Behavior |
| --- | --- |
| `static` (default) | Built-in PDO_ODBC in each enabled SAPI. |
| `no` | Explicit SDK-free opt-out; no native Db2 driver. |
| `shared` | Advanced matching-CLI-only module; incompatible SAPIs rejected. |

Native Db2 uses site-supplied `SDSNC.H` headers and the 64-bit
`SDSNMACS(DSNAO64C)` import side deck, not the LUW `clidriver`, unixODBC or
`-ldb2`. Follow the [python-ibmdb native installation reference][python-install]
and its [extraction/build code][python-build], with your DBA's actual dataset
names. Preserve header layout and original side-deck records. Never commit
or redistribute IBM SDK inputs, credentials, live qualifiers or site config.

SDK discovery, in priority order:

1. Explicit `PDO_ODBC_DB2_INCLUDE` and `PDO_ODBC_DB2_SIDEDECK` paths.
2. `PHP_DB2_SDK_ROOT/include` and `PHP_DB2_SDK_ROOT/lib/dsnao64c.x`.
3. `${ZOPEN_ROOTFS}/etc/php-db2-sdk/include` and its `lib/dsnao64c.x`.

The site SDK link is private configuration, not repository content. A missing
SDK fails the native build rather than silently omitting the driver. Use
`ZOPEN_PDO_ODBC=no` deliberately when building without it.

```sh
# Optional explicit root; unnecessary when the site link is provisioned.
export PHP_DB2_SDK_ROOT=/private/path/to/native-db2-sdk
zopen-build -v --no-set-active
```

Run builds in the foreground. Use a private `TMPDIR` for manual builds on a
CI account; older tools share a `${LOGNAME}.pipe` otherwise. When changing
mode or SDK paths, rebuild with `zopen-build -v -f`; cached mismatches are
rejected. Only clean up servers belonging to the isolated test checkout.

The SDK ABI probe uses the actual headers without calling Db2. The supported
native ABI has 64-bit pointers but **32-bit handles, SQLLEN and SQLULEN**;
do not substitute LUW/unixODBC LP64 types. No SDK input files belong in the
installed package.

## Runtime configuration

Users need their licensed native Db2 runtime, not SDK headers. Have the DBA
provide:

- `STEPLIB` with the approved Db2 load-library concatenation (commonly
  `SDSNEXIT`, `SDSNLOAD`, `SDSNLOD2`), unless the required modules are supplied
  through the site's normal loader configuration.
- `DSNAOINI` naming the approved native ODBC initialization file.
- A native datasource/location and authorization suitable for the application.

Set runtime variables in the trusted process environment **before PHP starts**.
For web applications, configure the CGI/FastCGI launcher or service manager;
do not take connection settings from HTTP input. `LIBPATH` does not replace
Db2 `STEPLIB`. The default built-in mode does not need a CLI-specific LIBPATH.

### DSNAOINI bytes and application encoding

The initialization file requires **EBCDIC IBM-1047 bytes and T=off**. Retagging
never converts ASCII/UTF-8 bytes. Convert a separate input of known encoding,
verify the actual output and keep the original private:

```sh
iconv -f ISO8859-1 -t IBM-1047 /private/odbc.ini.input > /private/odbc.ini.ebcdic
chtag -b /private/odbc.ini.ebcdic
export DSNAOINI=/private/odbc.ini.ebcdic
```

Use `-f UTF-8` for UTF-8 input. Avoid a second automatic conversion. Binary
or mixed IBM-1047 tagging with T=off is appropriate; an EBCDIC text tag can
trigger unwanted conversion. Protect both initialization and trace files.

Application-buffer encoding is a **different** setting. PHP strings are bytes;
configure the native CLI application CCSID with the DBA for the application's
encoding. The Python reference uses `CURRENTAPPENSCH=ASCII`, `FLOAT=IEEE` and
`MULTICONTEXT=2`, with RRSAF attachment in the subsystem stanza; these are
reference settings, not universally PHP-verified requirements. In particular,
a UTF-8 round trip alone can hide incorrect server-side transcoding. The live
suite creates a Unicode table and verifies `HEX(TXT)` as well as fetched bytes.

`PDO::ODBC_ATTR_ASSUME_UTF8=true` is **not** a native CCSID setting: its
UTF8/UCS2 conversion code is Windows-only. Native connection/statement setters
and constructor options reject true with `IM001`; false is allowed. Correctly
configured UTF-8 narrow-character buffers still require live acceptance.

### Connection-free checks

```sh
/path/to/candidate/bin/php -n -r 'var_dump(PDO::getAvailableDrivers());'
/path/to/candidate/bin/php -n --ri pdo_odbc
```

Both `odbc` and `sqlite` should be registered. This proves availability only,
not a successful call into Db2.

## Native data and safety behavior

- Forward fetching uses `SQLFetch`, matching the native Python LOB approach.
  Scroll offsets are checked for the native signed 32-bit API only where used.
- Distinct native `SQL_BLOB` uses binary binding and raw binary result requests.
  Standard SQL_BINARY/VARBINARY retain PDO_ODBC's existing hexadecimal result
  contract. Confirm the native metadata/result behavior with the live suite.
- Inputs use unknown-length data-at-execution, including nonstat streams and
  the current stream offset. Empty input sends an explicit zero-length put.
  Failed reads/puts stop immediately; native lengths are checked before casts.
- Output/input-output buffers retain a capacity independent of the returned
  indicator. Oversized/truncated values fail before an out-of-bounds copy.
- Native result readers reject ambiguous final lengths and short truncated
  character chunks instead of copying stale/padded data. The live Unicode/
  CLOB checks must establish whether the target runtime needs further handling.
- Missing UID/PWD components are completed independently with proper quoting;
  key recognition is case-insensitive and respects brace-quoted values.
- Native connections use driver cursors directly. Requests for ODBC
  driver-manager cursor emulation are rejected before runtime access.
- `PDO::ATTR_PERSISTENT=true` is explicitly unsupported (`IM001`) until safe
  request-end rollback/reset is implemented and validated. Use nonpersistent
  connections. Native driver-manager pooling is also disabled.

## Live Db2 acceptance

Use **only an approved disposable schema**. The suite creates a uniquely named
owned table containing Unicode text, BLOB and CLOB columns; it drops that table
on completion. Your DBA may need to provision database/tablespace permissions,
LOB auxiliary-object support, package binds and multi-connection authorization.
It never grants privileges, binds packages or changes site configuration.

Create a private environment file, using site values rather than these
placeholders, and restrict its access:

```sh
export STEPLIB='APPROVED.EXIT:APPROVED.LOAD:APPROVED.LOD2'
export DSNAOINI=/private/native-odbc.ini.ebcdic
export PDO_ODBC_TEST_DSN='odbc:DSN=APPROVED_TEST_LOCATION'
# USER/PASS can be omitted for administrator-approved trusted authentication.
export PDO_ODBC_TEST_USER='private-test-user'
export PDO_ODBC_TEST_PASS='private-test-password'
export PDO_ODBC_TEST_SCHEMA=APPROVED_DISPOSABLE_SCHEMA
export PHP_DB2_TEST_ALLOW_WRITES=yes
# Ensure native Db2 trace and LE dump destinations are private, too.
```

Do not commit that file or place passwords in command arguments/chat. Run:

```sh
sh tests/db2-acceptance.sh /private/db2-test-env.sh \
  /path/to/candidate/bin/php /path/to/candidate/bin/php-cgi
```

The wrapper uses private logs/working directories, bounded process timeouts,
and token-protected loopback CGI request variables. It does not expose a web
listener. This tests controlled CGI execution, not an Apache/FastCGI frontend
or its authentication/environment setup; the customer's real web deployment
must also be checked under its actual service identity. It prints no credentials, DSNs, SQL or vendor messages. Object
manifests are fully written/flushed before connecting and atomically updated.
They are retained privately if cleanup fails or a process is killed;
have the DBA remove only the listed test-owned objects in that case.

Required checks cover real native connections, rejected unsafe persistence,
prepared parameters, Unicode server bytes, raw binary/CLOB values, NULL versus
empty, buffer boundaries, remaining BLOB/CLOB stream offsets, unknown-length
BLOB/CLOB streams, independent connections, commit,
rollback, autocommit restoration and destruction rollback in **CLI and CGI**.
A missing approval/schema/DSN/runtime/driver refuses with exit 77, not a pass.
No real Db2 acceptance has been performed without those private inputs.

The build separately runs DB-free driver/parameter/fetch mocks, actual-SDK ABI
checks, safe refusal tests and SAPI registration. The focused upstream suite
allows zero failures but skips database tests without its DSN; those skips are
not database acceptance. Its Microsoft Access-specific fixture is Windows-only.
Full/mixed PHP runs retain the unchanged allowance of 112; interrupted runners
always fail explicitly regardless of that baseline.

## Diagnosis and references

- Loader failures: check approved runtime libraries and process startup env.
- SQLCODE `-805`: ask the DBA about package/plan binds and the customized
  `SDSNSAMP(DSNTIJCL)` job. Do not run it automatically from an application test.
- Native `APPLTRACE`/`DIAGTRACE` can contain SQL, data and connection details;
  keep destinations private and do not publish raw logs.
- [APAR PH41045][ph41045] is maintenance cited by the Python reference; DBA
  assessment is needed rather than asserting a universal PHP prerequisite.

[IBM Db2 ODBC Guide][ibm-odbc] and [python-ibmdb][python-install] are preparation
references, not substitutes for PHP customer validation.

[ibm-odbc]: https://www.ibm.com/support/knowledgecenter/SSEPEK/pdf/db2z_12_odbcbook.pdf
[ph41045]: https://www.ibm.com/support/pages/apar/PH41045
[python-install]: https://github.com/ibmdb/python-ibmdb/blob/master/INSTALL.md#inszos
[python-build]: https://github.com/ibmdb/python-ibmdb/blob/master/setup.py
