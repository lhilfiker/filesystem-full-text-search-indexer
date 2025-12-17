#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>


Index::DiskIO::DiskIO() = default;

bool Index::DiskIO::sync_file(mio::mmap_sink& target_mmap) const
{
  std::error_code ec;
  target_mmap.sync(ec);
  if (ec) {
    Log::write(4, "Index::DiskIO::sync_file: syncing failed.");
    return false;
  }
  return true;
}

bool Index::DiskIO::sync_all()
{
  if (!sync_file(mmap_paths)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  if (!sync_file(mmap_paths_count)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  if (!sync_file(mmap_words)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  if (!sync_file(mmap_words_f)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  if (!sync_file(mmap_reversed)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  if (!sync_file(mmap_additional)) {
    Log::write(4, "Index::DiskIO::sync_all: syncing failed for one file.");
    return false;
  }
  return true;
}


bool Index::DiskIO::map_file(mio::mmap_sink& target_mmap, size_t& target_size, const std::string& source_path)
{
  std::error_code ec;
  if (!std::filesystem::exists(source_path, ec)) {
    Log::write(4, "Index::DiskIO::map_file: could not map. index path invalid.");
    return false;
  }
  if (ec) {
    Log::write(4, "Index::DiskIO::map_file: could not map. validating path failed.");
    return false;
  }
  size_t temp_size = Helper::file_size(source_path);
  if (temp_size == 0) {
    Log::write(4, "Index::DiskIO::map_file: could not map. file size is 0");
    return false;
  }
  if (!target_mmap.is_mapped()) {
    target_mmap =
      mio::make_mmap_sink(source_path, 0,
                          mio::map_entire_file, ec);
    if (ec) {
      Log::write(4, "Index::DiskIO::map_file: could not map. mapping failed");
      target_size = 0;
      return false;
    }
  }
  else {
    Log::write(1, "Index::DiskIO::map_file: file already mapped.");
  }
  target_size = temp_size;
  Log::write(1, "Index::DiskIO::map_file: file successfully mapped.");
  return true;
}

bool Index::DiskIO::map(const std::filesystem::path& index_path)
{
  std::error_code ec;
  if (!std::filesystem::exists(index_path, ec)) {
    Log::write(4, "Index::DiskIO::map: could not map. index path invalid.");
    return false;
  }
  if (ec) {
    Log::write(4, "Index::DiskIO::map: could not map. index path validation threw an error");
    return false;
  }

  // paths.index
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
    Log::write(4, "Index::DiskIO::map: paths mapping failed.");
    unmap();
    return false;
  }
  // paths_count.index
  if (!map_file(mmap_paths_count, paths_count_size, (index_path / "paths_count.index").string())) {
    Log::write(4, "Index::DiskIO::map: paths_count mapping failed.");
    unmap();
    return false;
  }
  // words.index
  if (!map_file(mmap_words, words_size, (index_path / "words.index").string())) {
    Log::write(4, "Index::DiskIO::map: words mapping failed.");
    unmap();
    return false;
  }
  // words_f.index
  if (!map_file(mmap_words_f, words_f_size, (index_path / "words_f.index").string())) {
    Log::write(4, "Index::DiskIO::map: words_f mapping failed.");
    unmap();
    return false;
  }
  // reversed.index
  if (!map_file(mmap_reversed, reversed_size, (index_path / "reversed.index").string())) {
    Log::write(4, "Index::DiskIO::map: reversed mapping failed.");
    unmap();
    return false;
  }
  // additional.index
  // this one can fail: if there is no additional then it's size is 0.
  if (!map_file(mmap_additional, additional_size, (index_path / "additional.index").string())) {
    additional_mapped = false;
    Log::write(4, "Index::DiskIO::map: mapping failed for additional. This is allowed if it's size is 0.");
  }
  else {
    additional_mapped = true;
  }

  is_mapped = true;
  Log::write(1, "Index::DiskIO::map: mapped files successfully.");

  return true;
}

bool Index::DiskIO::unmap()
{
  sync_all();
  if (mmap_paths.is_mapped()) {
    mmap_paths.unmap();
  }
  if (mmap_paths_count.is_mapped()) {
    mmap_paths_count.unmap();
  }
  if (mmap_words.is_mapped()) {
    mmap_words.unmap();
  }
  if (mmap_words_f.is_mapped()) {
    mmap_words_f.unmap();
  }
  if (mmap_reversed.is_mapped()) {
    mmap_reversed.unmap();
  }
  if (mmap_additional.is_mapped()) {
    mmap_additional.unmap();
  }

  additional_mapped = false;
  is_mapped = false;
  Log::write(1, "Index::DiskIO::unmap: unmapped files successfully.");

  return true;
}
