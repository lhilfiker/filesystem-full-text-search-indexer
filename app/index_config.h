// index_config.h
#ifndef INDEX_CONFIG_H
#define INDEX_CONFIG_H

// Here you can change certain config options to further optimize towards your
// needs. Changing these values can reduce memory usage and speed up the systems
// or allow for even bigger indexes.

// Changing any of these values will entail a recreation of the index.

#define MAX_REVERSED_PATH_LINKS_AMOUNT 10
// This will change how much Path id links can be max in a reversed block.
// Changing this allows to set higher values in the config.
// Note: Due to the way it is programmed it will always create a max-amount
// object in memory. If setting the value lower in the config it means space is
// wasted that isn't used. This is why the precompiled program only allows a
// certain max size to find a good balance beetween average usecases and memory
// usage. On disk Index doesn't have unused space regardless of this setting.
#define DEFAULT_REVERSED_PATH_LINKS_AMOUNT 4
// This changes the default value of pathh links. This should NEVER be higher
// than the MAX_REVERSED_PATH_LINKS_AMOUNT

#define MAX_ADDITIONAL_PATH_LINKS_AMOUNT 50
// This will change how much Path id links can be max in a reversed block.
// Changing this allows to set higher values in the config.
// Note: Due to the way it is programmed it will always create a max-amount
// object in memory. If setting the value lower in the config it means space is
// wasted that isn't used. This is why the precompiled program only allows a
// certain max size to find a good balance beetween average usecases and memory
// usage. On disk Index doesn't have unused space regardless of this setting.
#define DEFAULT_ADDITIONAL_PATH_LINKS_AMOUNT 24
// This changes the default value of pathh links. This should NEVER be higher
// than the MAX_ADDITIONAL_PATH_LINKS_AMOUNT

#define MAX_PATH_ID_LINK_SIZE 4
// This changes the max Path ID Link size. A 4 byte value can
// hold up to 4,294,967,296 paths. If you plan to have more
// paths increase this to 8. Valid options are ONLY 2, 4 and 8.
// Note: If you increase this but not use it fully in the config it may lead to
// wasted space in memory.
#define DEFAULT_PATH_ID_LINK_SIZE 4
// This changes the default value. This should NEVER be higer than
// MAX_PATH_ID_LINK_SIZE

#define MAX_ADDITIONAL_ID_LINK_SIZE 4
// This changes the max Additional ID Link size. A 4 byte value can
// hold up to 4,294,967,296 additional blocks. If you plan to have more
// additional blocks increase this to 8. Valid options are ONLY 2, 4 and 8.
// Note: If you increase this but not use it fully in the config it may lead to
// wasted space in memory.
#define DEFAULT_ADDITIONAL_ID_LINK_SIZE 4
// This changes the default value. This should NEVER be higer than
// MAX_ADDITIONAL_ID_LINK_SIZE

#endif // INDEX_CONFIG_H