# Native Db2 candidate: validation and acceptance handoff

This is a **candidate**, not live Db2 acceptance. The published
`STABLE_phpport_462` package is SDK-free and has only SQLite PDO support.
The native branch has not been merged or released.

## Recorded evidence

Runtime source candidate `8b1a857` was built from a fresh Git clone, with
normal patch application and automatic private-SDK discovery. No allocator
bypass or Db2 mode/path override was used.

| Check | Result |
| --- | --- |
| Foreground full build/install/package | Exit 0 |
| Four-worker PHP suite | 11,482 pass; 7,136 skip; 93 fail |
| Full-suite allowance | Unchanged: 112 |
| Runner status | Complete; no BORK or observed S0C4 |
| Focused PDO_ODBC suite | 10 pass; 90 skip; zero fail; zero allowance |
| Actual SDK ABI, without API calls | Pointers 8 bytes; handles/SQLLEN/SQLULEN 4 bytes |
| Extracted package, second host | 101 selected PHPT pass; 92 skip; zero fail |
| General runtime smoke groups | 12 pass, including SQLite-only LOB/transaction checks |
| CLI, controlled CGI, phpdbg | Built-in `odbc,sqlite`, with no extension setting |
| Package SDK audit | 304 files checked; no raw private SDK input hashes matched |

The full-suite allowance does not mean all 93 remaining failures are harmless.
Skipped database tests are **not** database acceptance. General SQLite tests
say nothing about native Db2 behavior.

The original tested archive is
`php-8.2.34.20261016_224357.zos.pax.Z`, SHA-256:

```text
42f12e62f44f4014639fdd6765c09043cf201fb63340a7f8f277371e6eb851ab
```

The updated package from recipe `c48db8e` is
`php-8.2.34.20261017_005122.zos.pax.Z`, SHA-256:

```text
84ca34ddd77d312591828eb9f0300407d6249f68f2e8f8082fbc438efeb544cd
```

Its foreground build/install/package exited 0, including the new post-install
gate and focused 10-pass/90-skip/zero-failure suite. Extracting it on the second
host and running its embedded `install_test.sh` also passed. All three runtime
executables are **byte-identical** to the full-suite-validated `8b1a857` package;
its metadata accurately reports the new focused 10/10 run, not the earlier full
suite. Its 306-file audit matched no raw SDK input hashes. The setup/acceptance
documents are included. Neither archive is an official production release.

The original archive above predates the real install checks. Do not relabel its
hash as a newer artifact. The installed-package probe checks startup, native driver registration and
small engine/SQLite operations, without an SDK or Db2 calls. It is embedded
into new packages and run after installation during the build. For an existing
extracted candidate, run it explicitly:

```sh
sh tests/validate-install.sh /absolute/candidate/root static 8.2.34
```

`no` validates an SDK-free package; `shared` validates only its matching CLI.
The normal native delivery target is `static`, including CGI and phpdbg.
No production PHP activation is necessary to run these checks.

## Required live acceptance inputs

Have the DBA/operator provide a **private trusted environment file**, not
passwords in an issue or chat. It must identify the approved runtime,
EBCDIC/T=off DSNAOINI, native test DSN and authorized disposable schema.
Use a dedicated account restricted to test-owned objects where possible.
The schema denylist is a safety guard, not proof that a schema is disposable;
its approval remains the operator's responsibility.

See [the setup and live-test instructions](pdo-odbc.md#live-db2-acceptance).
With explicitly approved writes, run:

```sh
sh tests/db2-acceptance.sh /private/approved-db2-env.sh \
  /absolute/candidate/root/bin/php /absolute/candidate/root/bin/php-cgi
```

The harness requires explicit `PHP_DB2_TEST_ALLOW_WRITES=yes`. Missing approval
or configuration refuses with exit 77; it is not a successful acceptance run.
It creates only a randomized owned table in the approved schema. Private
manifests precede DB actions; interrupted/time-out runs can require DBA
reconciliation of the listed objects. Never remove an entire schema or an
unrelated object to clean up this test.

Required evidence: real connection, prepared parameters, server-side Unicode
bytes, binary/CLOB/NULL/empty/boundary/stream round trips, commit/rollback,
autocommit restoration, destruction rollback and owned-object cleanup in both
CLI and CGI. Keep raw vendor logs and native traces private.

Controlled CGI is **not** deployed-web acceptance. Repeat representative
operations through the customer's actual frontend under its service identity,
including startup environment, encoding and authorization. FPM is unvalidated.
Unsafe PDO persistence and the Windows-only wide-conversion flag are rejected;
check whether existing applications depend on those options.

## Review and release gates

- Open a review PR for `native-db2-8234-candidate`; branch pushes alone do not
  trigger this repository's pull-request CI workflow.
- Verify actual automated CI on the reviewed SHA, not only the manual CI-host
  build recorded above. Obtain required code-owner approval without bypasses.
- Obtain successful live CLI/CGI results and customer deployment evidence.
- Publish through the normal approved release process, then verify the actual
  installed release contains built-in ODBC and repeat acceptance there.
- Keep issue #39 open; show any issue-comment draft for approval before posting.
