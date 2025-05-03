// index_config.h
#ifndef INDEX_CONFIG_H
#define INDEX_CONFIG_H

// Here you can change certain config options to further optimize towards your
// needs. Changing these values can reduce memory usage and speed up the systems
// or allow for even bigger indexes.

// Changing any of these values will entail a recreation of the index.

#define PATH_ID_LINK_SIZE 2
// This changes the Path ID Link size. A 2 byte value can hold up to 65536 paths
// and a 4 byte value can hold up to 4,294,967,296 paths. Default is 2. If you
// plan to have more paths increase this to 4 or 8. Valid options are ONLY 2, 4
// and 8.

#define ADDITIONAL_ID_LINK_SIZE 4
// This changes the Additional ID Link size. A 4 byte value can
// hold up to 4,294,967,296 additional blocks. If you plan to have more
// additional blocks increase this to 8. Valid options are ONLY 2, 4 and 8.

// Further Optimizations
#define REVERSED_PATH_LINKS_AMOUNT 4
// This will change how much Path id links are in a reversed block.
// Changing this allows for more path ids per reversed block. Useful if you know
// more paths will be connected to each word without needing to waste space on
// an addtional.

#define ADDITIONAL_PATH_LINKS_AMOUNT 24
// This will change how much Path id links are in a additional block.
// Changing this allows for more path ids per additional block.

#endif // INDEX_CONFIG_H