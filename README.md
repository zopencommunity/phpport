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

- [Native Db2 PDO_ODBC on z/OS (experimental, opt-in)](docs/pdo-odbc.md)

# Runtime and CI status

PHP **8.2.34** includes native aligned Zend chunks and LE-compatible fiber
stacks instead of the unsafe anonymous-mmap shim. A foreground build/install
on the CI host exited **0** without the reported PCRE cleanup S0C4: **11,469
passes, 7,059 skips and 96 failures**, within the **unchanged allowance of
112**. Remaining failures are not all declared harmless.

Interrupted/crashed test runners now retain partial results and fail explicitly,
including crashes after a printed summary; they cannot report misleading zero
totals or hide under the ordinary failure allowance.

This CI/runtime fix does not enable Db2 by default or claim working database
operations. Built-in multi-SAPI PDO_ODBC delivery is a separate candidate;
real Db2 query, LOB and transaction acceptance is still outstanding.

# Contributing
Contributions are welcome! Please follow the [zopen contribution guidelines](https://github.com/zopencommunity/meta/blob/main/CONTRIBUTING.md).
