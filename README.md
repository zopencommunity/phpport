[![Automatic version updates](https://github.com/ZOSOpenTools/phpport/actions/workflows/bump.yml/badge.svg)](https://github.com/ZOSOpenTools/phpport/actions/workflows/bump.yml)

# Php

A programming language

# Installation and Usage

Use the zopen package manager ([QuickStart Guide](https://zopen.community/#/Guides/QuickStart)) to install:
```bash
zopen install php
```

# Building from Source

1. Clone the repository:
```bash
git clone https://github.com/zopencommunity/phpport.git
cd phpport
```
2. Build using zopen:
```bash
zopen build -vv
```

See the [zopen porting guide](https://zopen.community/#/Guides/Porting) for more details.

# Documentation

- [Native Db2 PDO_ODBC build, runtime setup and acceptance](docs/pdo-odbc.md)

# Runtime and CI status

PHP **8.2.34** includes native aligned Zend chunks and LE-compatible fiber
stacks instead of the unsafe anonymous-mmap shim. A foreground build/install
on the CI host exited **0** without the reported PCRE cleanup S0C4: **11,469
passes, 7,059 skips and 96 failures**, within the **unchanged allowance of
112**. Remaining failures are not all declared harmless.

Interrupted/crashed test runners now retain partial results and fail explicitly,
including crashes after a printed summary; they cannot report misleading zero
totals or hide under the ordinary failure allowance.

## Native Db2 delivery candidate - issue #39

This branch defaults to building **PDO_ODBC into CLI, CGI/FastCGI and phpdbg**.
No extension setting or CLI-specific module is required. CI discovers its
private SDK through `${ZOPEN_ROOTFS}/etc/php-db2-sdk`; SDK-free source builds
must explicitly set `ZOPEN_PDO_ODBC=no`. IBM headers, the import side deck,
credentials and site configuration are never distributed.

**Candidate, not a Db2-validated release:** the published STABLE_phpport_462
package still has only the SQLite PDO driver. The native candidate builds and
packages successfully, but connections, Unicode, prepared LOBs and transactions
still require the approved live tests described in
[the acceptance guide](docs/pdo-odbc.md#live-db2-acceptance).

The candidate hardens native BLOB handling, streamed parameters, buffer/length
bounds and credential completion. Persistent connections and the Windows-only
`ODBC_ATTR_ASSUME_UTF8` conversion flag fail explicitly rather than pretending
to support unsafe behavior. Registration and skipped database tests do not
establish customer acceptance; issue #39 remains open.

# Contributing
Contributions are welcome! Please follow the [zopen contribution guidelines](https://github.com/zopencommunity/meta/blob/main/CONTRIBUTING.md).
