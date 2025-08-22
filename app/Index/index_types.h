// index_types.h
#ifndef INDEX_TYPES_H
#define INDEX_TYPES_H

#include "../index_config.h"
#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// First define the size based on the path ID type
#if PATH_ID_LINK_SIZE == 2
#define PATH_ID_TYPE uint16_t
#elif PATH_ID_LINK_SIZE == 4
#define PATH_ID_TYPE uint32_t
#elif PATH_ID_LINK_SIZE == 8
#define PATH_ID_TYPE uint64_t
#else
#error "PATH_ID_LINK_SIZE must be 2, 4, or 8"
#endif

// Define the additional ID type
#if ADDITIONAL_ID_LINK_SIZE == 2
#define ADDITIONAL_ID_TYPE uint16_t
#elif ADDITIONAL_ID_LINK_SIZE == 4
#define ADDITIONAL_ID_TYPE uint32_t
#elif ADDITIONAL_ID_LINK_SIZE == 8
#define ADDITIONAL_ID_TYPE uint64_t
#else
#error "ADDITIONAL_ID_LINK_SIZE must be 2, 4, or 8"
#endif

// Define the Words_f Location type
#if WORDS_F_LOCATION_SIZE == 2
#define WORDS_F_LOCATION_TYPE uint16_t
#elif WORDS_F_LOCATION_SIZE == 4
#define WORDS_F_LOCATION_TYPE uint32_t
#elif WORDS_F_LOCATION_SIZE == 8
#define WORDS_F_LOCATION_TYPE uint64_t
#else
#error "WORDS_F_LOCATION_SIZE must be 2, 4, or 8"
#endif

// Define the Word Seperator type
#if WORD_SEPARATOR_SIZE == 1
#define WORD_SEPARATOR_TYPE uint8_t
#elif WORD_SEPARATOR_SIZE == 2
#define WORD_SEPARATOR_TYPE uint16_t
#elif WORD_SEPARATOR_SIZE == 4
#define WORD_SEPARATOR_TYPE uint32_t
#else
#error "WORD_SEPARATOR_SIZE must be 1, 2, or 4"
#endif

#define REVERSED_ENTRY_SIZE                                                    \
  (REVERSED_PATH_LINKS_AMOUNT * PATH_ID_LINK_SIZE + ADDITIONAL_ID_LINK_SIZE)
#define ADDITIONAL_ENTRY_SIZE                                                  \
  (ADDITIONAL_PATH_LINKS_AMOUNT * PATH_ID_LINK_SIZE + ADDITIONAL_ID_LINK_SIZE)

#pragma pack(push, 1)
union TransactionHeader {
  struct {
    uint8_t status;     // 0 = no status, 1 = started, 2 = completed
    uint8_t index_type; // paths = 0, words = 1, words_f = 2, reversed = 3,
                        // additional = 4, paths_count = 5
    uint64_t location;  // byte count in file, resize = 0.
    uint64_t backup_id; // 0 = none. starting from 1 for  each transaction. each
                        // transaction has its own unique folder. same id for
                        // move and create a backup operation.
    uint8_t
        operation_type; // 0 = Move, 1 = write, 2 = resize, 3 = create a backup
    uint64_t content_length; // length of content. for resize this indicates the
                             // new size. for move how much from the starting
                             // point and the same for create a backup.
  };
  unsigned char bytes[27];
};
#pragma pack(pop)

struct Transaction {
  TransactionHeader header;
  std::string content; // For move operations this will be 8 bytes always and it
                       // is a uint64_t and signals the byte shift.
};

#pragma pack(push, 1)
union InsertionHeader {
  struct {
    uint64_t location;
    uint64_t content_length;
  };
  unsigned char bytes[16];
};
#pragma pack(pop)

struct Insertion {
  InsertionHeader header;
  std::string content;
};

struct PathsMapping {
  std::unordered_map<PATH_ID_TYPE, PATH_ID_TYPE> by_local;
  std::unordered_map<PATH_ID_TYPE, PATH_ID_TYPE> by_disk;
};

#pragma pack(push, 1)
union WordsFValue {
  struct {
    uint64_t location;
    WORDS_F_LOCATION_TYPE id;
  };
  unsigned char bytes[8 + WORDS_F_LOCATION_SIZE];
};
#pragma pack(pop)

// ReversedBlock with depending on compiletime config different sizes.
#pragma pack(push, 1)

union ReversedBlock {
  struct {
    PATH_ID_TYPE path[REVERSED_PATH_LINKS_AMOUNT];
    ADDITIONAL_ID_TYPE additional[1];
  } ids;
  unsigned char bytes[(REVERSED_PATH_LINKS_AMOUNT * PATH_ID_LINK_SIZE) +
                      ADDITIONAL_ID_LINK_SIZE];
};
#pragma pack(pop)

#pragma pack(push, 1)

union WordSeperator {
  WORD_SEPARATOR_TYPE seperator;
  unsigned char bytes[WORD_SEPARATOR_SIZE];
};
#pragma pack(pop)

#pragma pack(push, 1)
union AdditionalBlock {
  struct {
    PATH_ID_TYPE path[ADDITIONAL_PATH_LINKS_AMOUNT];
    ADDITIONAL_ID_TYPE additional[1];
  } ids;
  unsigned char bytes[(ADDITIONAL_PATH_LINKS_AMOUNT * PATH_ID_LINK_SIZE) +
                      ADDITIONAL_ID_LINK_SIZE];
};
#pragma pack(pop)

// Path Offsets
#pragma pack(push, 1)
union PathOffset {
  uint16_t offset;
  unsigned char bytes[2];
};
#pragma pack(pop)

// PathIDOffset with depending on compiletime config different sizes.
#pragma pack(push, 1)

#if PATH_ID_LINK_SIZE == 2
union PathIDOffset {
  uint16_t offset;
  unsigned char bytes[2];
};
#elif PATH_ID_LINK_SIZE == 4
union PathIDOffset {
  uint32_t offset;
  unsigned char bytes[4];
};
#elif PATH_ID_LINK_SIZE == 8
union PathIDOffset {
  uint64_t offset;
  unsigned char bytes[8];
};
#else
#error "PATH_ID_LINK_SIZE must be 2, 4, or 8"
#endif
#pragma pack(pop)

#pragma pack(push, 1)

#if ADDITIONAL_ID_LINK_SIZE == 2
union AdditionalOffset {
  uint16_t offset;
  unsigned char bytes[2];
};
#elif ADDITIONAL_ID_LINK_SIZE == 4
union AdditionalOffset {
  uint32_t offset;
  unsigned char bytes[4];
};
#elif ADDITIONAL_ID_LINK_SIZE == 8
union AdditionalOffset {
  uint64_t offset;
  unsigned char bytes[8];
};
#else
#error "ADDITIONAL_ID_LINK_SIZE must be 2, 4, or 8"
#endif
#pragma pack(pop)

#pragma pack(push, 1)
union PathsCountItem {
  uint32_t num;
  unsigned char bytes[4];
};
#pragma pack(pop)

#pragma pack(push, 1)
union MoveOperationContent {
  uint64_t num;
  unsigned char bytes[8];
};
#pragma pack(pop)

struct search_path_ids_count_return {
  std::vector<PATH_ID_TYPE> path_id;
  std::vector<uint32_t> count;
};

#endif