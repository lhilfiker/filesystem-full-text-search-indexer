# filesystem-full-text-search-indexer

 A multithreaded file indexer for word searches with a self-developed index structure for minimal memory footprint and self-healing, transaction-based updates for complete index correctness even during an interruption and instant recovery. 
 The search function provides a powerful boolean query language supporting AND, OR, and NOT operators with nested parentheses for complex queries, along with both wildcard (partial match) and exact match capabilities using quoted terms. Search results are ranked by match frequency and can be configured for minimum character requirements and exact/wildcard matching behaviour.

 The project is currently still in development and not really usable. The most features are already there. For the first 'alpha' release the following items need to be completed first:

 - config file #48 , PR: #107
 - Updated Files (detect changed files for only partial indexing) #99
 - cli: initial functionality (sub-stories will follow after a design is available.)
 - Build System #103
 - Better Readme with introduction, feature list, installation instruction, build instruction, etc..
 
 Those are also tracked in the Milestone: https://github.com/lhilfiker/filesystem-full-text-search-indexer/milestone/1


 After the v.0.1 release the following tasks are available, mainly focusing on improving maintainability, refactoring, testing and adding some new features: 
 https://github.com/lhilfiker/filesystem-full-text-search-indexer/milestone/2

