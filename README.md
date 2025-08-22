# filesystem-full-text-search-indexer

A multithreaded file indexer for word searches with a self-developed index structure for minimal memory footprint and self-healing, transaction-based updates for complete index correctness even during an interruption and instant recovery. Features an inter-process locking mechanism to prevent data corruption during concurrent access. Designed to handle large datasets efficiently with memory-mapped file I/O and optimized binary storage formats that scale to millions of files and gigabytes of text content. The search function provides a powerful boolean query language supporting AND, OR, and NOT operators with nested parentheses for complex queries, along with both wildcard (partial match) and exact match capabilities using quoted terms. Includes alphabet jump tables for search optimization and configurable data types to minimize memory usage based on dataset size requirements. Search results are ranked by match frequency and can be configured for minimum character requirements and exact/wildcard matching behaviour.

 The project is currently still in development and not really usable. The most features are already there. For the first 'alpha' release the following items need to be completed first:

 - config file [#48](https://github.com/lhilfiker/filesystem-full-text-search-indexer/issues/48) , PR: [#107](https://github.com/lhilfiker/filesystem-full-text-search-indexer/pull/107)
 - Updated Files (detect changed files for only partial indexing) [#99](https://github.com/lhilfiker/filesystem-full-text-search-indexer/issues/99)
 - cli: initial functionality (sub-stories will follow after a design is available.)
 - Build System [#103](https://github.com/lhilfiker/filesystem-full-text-search-indexer/issues/103)
 - Better Readme with introduction, feature list, installation instruction, build instruction, etc..
 
 Those are also tracked in the Milestone: https://github.com/lhilfiker/filesystem-full-text-search-indexer/milestone/1


 After the v.0.1 release the following tasks are available, mainly focusing on improving maintainability, refactoring, testing and adding some new features and performance and configurations improvements to support even larger datasets(TBs+ in size): 
 https://github.com/lhilfiker/filesystem-full-text-search-indexer/milestone/2

Metadata per file, integrations are also planned and tracked in their respective issues.
