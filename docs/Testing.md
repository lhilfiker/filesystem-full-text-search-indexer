# Integration Testing Documentation

## Overview

The program is being tested the following ways:

### 1. Build Tests

The program is automatically built on each push. This just validates that there are no build issues.

### 2. Integration Tests

The program is also tested using integration tests for basic functionality such as indexing, merging, search, query
search with a small test set (200MB). More details can be found [Integration-Tests.md](Integration-Tests.md).

## Planned

Crash Tests for the merging and transaction execution are planned and tracked in issue #158

Unit tests are also planned as well as other types of tests. Especially tests around reliability are planned.

- Lock tests
- Performance tests
- Fuzz tests