// indexer_types.h
#ifndef INDEXER_TYPES_H
#define INDEXER_TYPES_H

#include "../LocalIndex/localindex.h"
#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct threads_jobs {
  std::future<LocalIndex> future;
  uint32_t queue_id;
};

#endif