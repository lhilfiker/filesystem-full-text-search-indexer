#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

Index::DiskIO::DiskIO()
  : paths_size(0)
    , paths_count_size(0)
    , words_size(0)
    , words_f_size(0)
    , reversed_size(0)
    , additional_size(0)
    , additional_mapped(0)
    , is_mapped(false)
    , is_locked(false)
{
}

bool Index::DiskIO::sync_file(mio::mmap_sink& target_mmap)
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
  // Because sync may be called anytime, we will not check if the operation failed.
  sync_file(mmap_paths);
  sync_file(mmap_paths_count);
  sync_file(mmap_words);
  sync_file(mmap_words_f);
  sync_file(mmap_reversed);
  sync_file(mmap_additional);
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
    Log::write(4, "Index::DiskIO::map: mapping failed.");
    unmap();
    return false;
  }
  // paths_count.index
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
    Log::write(4, "Index::DiskIO::map: mapping failed.");
    unmap();
    return false;
  }
  // words.index
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
    Log::write(4, "Index::DiskIO::map: mapping failed.");
    unmap();
    return false;
  }
  // words_f.index
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
    Log::write(4, "Index::DiskIO::map: mapping failed.");
    unmap();
    return false;
  }
  // reversed.index
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
    Log::write(4, "Index::DiskIO::map: mapping failed.");
    unmap();
    return false;
  }
  // additional.index
  // this one can fail: if there is no additional then it's size is 0.
  if (!map_file(mmap_paths, paths_size, (index_path / "paths.index").string())) {
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
