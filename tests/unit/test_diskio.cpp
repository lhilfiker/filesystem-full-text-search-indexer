// Note: Unit tests were mostly generated with the help of AI.

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstring>

// Pull in the full Index class (and its private DiskIO nested class).
// We are friended as DiskIOTest so we can reach DiskIO directly.
#include "../../app/Index/index.h"
#include "../../app/Index/index_types.h"
#include "../../app/index_config.h"

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Write exactly `size` zeroed bytes to `path` so mmap can map it.
static void create_file(const fs::path& p, size_t size)
{
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(f.is_open()) << "Could not create: " << p;
  if (size > 0) {
    std::vector<char> buf(size, '\0');
    f.write(buf.data(), size);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test fixture
//
// Each test gets a fresh temp directory with properly-sized stub index files.
// Sizes are chosen to hold a small but non-trivial amount of data so every
// getter / setter can be exercised without hitting the bounds-check asserts.
// ─────────────────────────────────────────────────────────────────────────────
class DiskIOTest : public ::testing::Test
{
protected:
  fs::path dir_;
  Index::DiskIO dio_; // friended — we can construct it here

  // Sizing constants (keep small for tests)
  static constexpr size_t N_PATHS = 4; // number of path slots
  static constexpr size_t N_WORDS = 4; // number of word entries
  static constexpr size_t N_REVERSED = 4; // one reversed block per word
  static constexpr size_t N_ADDITIONAL = 4; // additional blocks

  // Derived file sizes
  // paths.index: each slot = 2-byte length prefix + up to 64 bytes
  static constexpr size_t PATHS_SIZE = N_PATHS * (2 + 16);
  static constexpr size_t PATHS_COUNT_SIZE = N_PATHS * 4;
  static constexpr size_t WORDS_F_SIZE = 26 * (8 + WORDS_F_LOCATION_SIZE);
  static constexpr size_t REVERSED_SIZE = N_REVERSED * REVERSED_ENTRY_SIZE;
  static constexpr size_t ADDITIONAL_SIZE = N_ADDITIONAL * ADDITIONAL_ENTRY_SIZE;

  // words.index: 4 words, each = WORD_SEPARATOR_SIZE + word bytes
  //  word lengths: 5, 6, 7, 8
  static constexpr size_t WORDS_SIZE =
    (WORD_SEPARATOR_SIZE + 5) +
    (WORD_SEPARATOR_SIZE + 6) +
    (WORD_SEPARATOR_SIZE + 7) +
    (WORD_SEPARATOR_SIZE + 8);

  void SetUp() override
  {
    dir_ = fs::temp_directory_path() / ("diskio_test_" + std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir_);

    create_file(dir_ / "paths.index", PATHS_SIZE);
    create_file(dir_ / "paths_count.index", PATHS_COUNT_SIZE);
    create_file(dir_ / "words.index", WORDS_SIZE);
    create_file(dir_ / "words_f.index", WORDS_F_SIZE);
    create_file(dir_ / "reversed.index", REVERSED_SIZE);
    create_file(dir_ / "additional.index", ADDITIONAL_SIZE);

    ASSERT_TRUE(dio_.map(dir_)) << "DiskIO::map failed";
  }

  void TearDown() override
  {
    dio_.unmap();
    fs::remove_all(dir_);
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// map / unmap / get_mapped
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, MapReportsCorrectly)
{
  EXPECT_TRUE(dio_.get_mapped());
}

TEST_F(DiskIOTest, UnmapClearsFlag)
{
  EXPECT_TRUE(dio_.unmap());
  EXPECT_FALSE(dio_.get_mapped());
}

TEST_F(DiskIOTest, RemapAfterUnmap)
{
  dio_.unmap();
  EXPECT_TRUE(dio_.map(dir_));
  EXPECT_TRUE(dio_.get_mapped());
}

TEST_F(DiskIOTest, MapFailsOnNonexistentDirectory)
{
  Index::DiskIO fresh;
  EXPECT_FALSE(fresh.map(dir_ / "does_not_exist"));
  EXPECT_FALSE(fresh.get_mapped());
}

// ─────────────────────────────────────────────────────────────────────────────
// Size getters
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, GetPathsSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_paths_size(), PATHS_SIZE);
}

TEST_F(DiskIOTest, GetPathsCountSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_paths_count_size(), PATHS_COUNT_SIZE);
}

TEST_F(DiskIOTest, GetWordsSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_words_size(), WORDS_SIZE);
}

TEST_F(DiskIOTest, GetWordsFSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_words_f_size(), WORDS_F_SIZE);
}

TEST_F(DiskIOTest, GetReversedSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_reversed_size(), REVERSED_SIZE);
}

TEST_F(DiskIOTest, GetAdditionalSizeMatchesFile)
{
  EXPECT_EQ(dio_.get_additional_size(), ADDITIONAL_SIZE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Path separator (2-byte length prefix in paths.index)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, PathSeparatorRoundTrip)
{
  const uint16_t len = 12;
  dio_.set_path_separator(0, len);
  EXPECT_EQ(dio_.get_path_separator(0), len);
}

TEST_F(DiskIOTest, PathSeparatorMultipleSlots)
{
  // Write to several non-overlapping offsets and verify independence
  dio_.set_path_separator(0, 3);
  dio_.set_path_separator(18, 7); // 2 + 16 = 18 per slot
  dio_.set_path_separator(36, 11);

  EXPECT_EQ(dio_.get_path_separator(0), 3);
  EXPECT_EQ(dio_.get_path_separator(18), 7);
  EXPECT_EQ(dio_.get_path_separator(36), 11);
}

// ─────────────────────────────────────────────────────────────────────────────
// Path string
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, PathStringRoundTrip)
{
  const std::string path = "/home/user/doc.txt";
  const uint16_t len = static_cast<uint16_t>(path.size());

  dio_.set_path_separator(0, len);
  dio_.set_path(2, path, len); // path data starts after the 2-byte sep

  EXPECT_EQ(dio_.get_path(2, len), path);
}

TEST_F(DiskIOTest, PathStringOverwrite)
{
  const std::string first = "aaa";
  const std::string second = "zzz";
  const uint16_t len = 3;

  dio_.set_path(2, first, len);
  dio_.set_path(2, second, len);

  EXPECT_EQ(dio_.get_path(2, len), second);
}

// ─────────────────────────────────────────────────────────────────────────────
// Path count (4-byte word-count per path, 1-indexed)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, PathCountRoundTrip)
{
  dio_.set_path_count(1, 42u);
  EXPECT_EQ(dio_.get_path_count(1), 42u);
}

TEST_F(DiskIOTest, PathCountMultipleSlots)
{
  for (size_t i = 1; i <= N_PATHS; ++i) {
    dio_.set_path_count(i, static_cast<uint32_t>(i * 100));
  }
  for (size_t i = 1; i <= N_PATHS; ++i) {
    EXPECT_EQ(dio_.get_path_count(i), static_cast<uint32_t>(i * 100));
  }
}

TEST_F(DiskIOTest, PathCountZeroIsValid)
{
  dio_.set_path_count(1, 999u);
  dio_.set_path_count(1, 0u);
  EXPECT_EQ(dio_.get_path_count(1), 0u);
}

TEST_F(DiskIOTest, PathCountMaxUint32)
{
  const uint32_t maxVal = std::numeric_limits<uint32_t>::max();
  dio_.set_path_count(2, maxVal);
  EXPECT_EQ(dio_.get_path_count(2), maxVal);
}

// ─────────────────────────────────────────────────────────────────────────────
// Word separator
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, WordSeparatorRoundTrip)
{
  // First word starts at byte 0.
  const WORD_SEPARATOR_TYPE sep = 5;
  dio_.set_word_separator(0, sep);
  EXPECT_EQ(dio_.get_word_separator(0), sep);
}

TEST_F(DiskIOTest, WordSeparatorSecondWord)
{
  // Second word starts after first: WORD_SEPARATOR_SIZE + 5 chars
  const size_t offset = WORD_SEPARATOR_SIZE + 5;
  const WORD_SEPARATOR_TYPE sep = 6;
  dio_.set_word_separator(offset, sep);
  EXPECT_EQ(dio_.get_word_separator(offset), sep);
}

// ─────────────────────────────────────────────────────────────────────────────
// Individual chars in words
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, GetCharOfWordAfterSetWord)
{
  const std::string word = "hello";
  dio_.set_word_separator(0, static_cast<WORD_SEPARATOR_TYPE>(word.size()));
  dio_.set_word(WORD_SEPARATOR_SIZE, word, word.size());

  for (uint16_t i = 0; i < word.size(); ++i) {
    EXPECT_EQ(dio_.get_char_of_word(0, i), word[i])
      << "Mismatch at char index " << i;
  }
}

TEST_F(DiskIOTest, GetCharOfWordSecondWord)
{
  // Layout: [sep5][hello][sep5][world]
  const std::string w1 = "hello";
  const std::string w2 = "world";
  const size_t off2 = WORD_SEPARATOR_SIZE + w1.size();

  dio_.set_word_separator(0, static_cast<WORD_SEPARATOR_TYPE>(w1.size()));
  dio_.set_word(WORD_SEPARATOR_SIZE, w1, w1.size());

  dio_.set_word_separator(off2, static_cast<WORD_SEPARATOR_TYPE>(w2.size()));
  dio_.set_word(off2 + WORD_SEPARATOR_SIZE, w2, w2.size());

  EXPECT_EQ(dio_.get_char_of_word(off2, 0), 'w');
  EXPECT_EQ(dio_.get_char_of_word(off2, 4), 'd');
}

// ─────────────────────────────────────────────────────────────────────────────
// words_f (alphabet jump table — 26 WordsFValue entries)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, WordsFRoundTrip)
{
  std::vector<WordsFValue> wf(26);
  for (int i = 0; i < 26; ++i) {
    wf[i].location = static_cast<uint64_t>(i * 1000);
    wf[i].id = static_cast<WORDS_F_LOCATION_TYPE>(i);
  }
  dio_.set_words_f(wf);

  auto back = dio_.get_words_f();
  ASSERT_EQ(back.size(), 26u);
  for (int i = 0; i < 26; ++i) {
    EXPECT_EQ(back[i].location, static_cast<uint64_t>(i * 1000))
      << "location mismatch for letter " << static_cast<char>('a' + i);
    EXPECT_EQ(back[i].id, static_cast<WORDS_F_LOCATION_TYPE>(i))
      << "id mismatch for letter " << static_cast<char>('a' + i);
  }
}

TEST_F(DiskIOTest, WordsFAllZeroesIsValid)
{
  std::vector<WordsFValue> wf(26); // value-initialized to 0
  dio_.set_words_f(wf);
  auto back = dio_.get_words_f();
  for (int i = 0; i < 26; ++i) {
    EXPECT_EQ(back[i].location, 0u);
    EXPECT_EQ(back[i].id, 0u);
  }
}

TEST_F(DiskIOTest, WordsFOverwrite)
{
  std::vector<WordsFValue> wf(26);
  wf[0].location = 999;
  wf[0].id = 1;
  dio_.set_words_f(wf);

  wf[0].location = 42;
  wf[0].id = 2;
  dio_.set_words_f(wf);

  auto back = dio_.get_words_f();
  EXPECT_EQ(back[0].location, 42u);
  EXPECT_EQ(back[0].id, 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reversed blocks
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, ReversedBlockRoundTrip)
{
  ReversedBlock rb{};
  rb.ids.path[0] = 1;
  rb.ids.path[1] = 2;
  rb.ids.additional[0] = 0;

  dio_.set_reversed(rb, 0);

  ReversedBlock* got = dio_.get_reversed_pointer(0);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(got->ids.path[0], 1u);
  EXPECT_EQ(got->ids.path[1], 2u);
  EXPECT_EQ(got->ids.additional[0], 0u);
}

TEST_F(DiskIOTest, ReversedBlockMultipleEntries)
{
  for (size_t i = 0; i < N_REVERSED; ++i) {
    ReversedBlock rb{};
    rb.ids.path[0] = static_cast<PATH_ID_TYPE>(i + 1);
    dio_.set_reversed(rb, i);
  }
  for (size_t i = 0; i < N_REVERSED; ++i) {
    ReversedBlock* got = dio_.get_reversed_pointer(i);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->ids.path[0], static_cast<PATH_ID_TYPE>(i + 1))
      << "Mismatch at reversed entry " << i;
  }
}

TEST_F(DiskIOTest, ReversedBlockAllPathSlots)
{
  // Fill every path slot and verify all are preserved
  ReversedBlock rb{};
  for (uint16_t i = 0; i < REVERSED_PATH_LINKS_AMOUNT; ++i) {
    rb.ids.path[i] = static_cast<PATH_ID_TYPE>(i + 10);
  }
  rb.ids.additional[0] = 0;
  dio_.set_reversed(rb, 0);

  ReversedBlock* got = dio_.get_reversed_pointer(0);
  for (uint16_t i = 0; i < REVERSED_PATH_LINKS_AMOUNT; ++i) {
    EXPECT_EQ(got->ids.path[i], static_cast<PATH_ID_TYPE>(i + 10))
      << "Path slot " << i << " wrong";
  }
}

TEST_F(DiskIOTest, ReversedBlockWithAdditionalLink)
{
  ReversedBlock rb{};
  rb.ids.path[0] = 1;
  rb.ids.additional[0] = 2; // points to additional block #2
  dio_.set_reversed(rb, 0);

  ReversedBlock* got = dio_.get_reversed_pointer(0);
  EXPECT_EQ(got->ids.additional[0], 2u);
}

TEST_F(DiskIOTest, ReversedPointerWritebackViaPtrAffectsDisk)
{
  // get_reversed_pointer returns a pointer into the mmap,
  // so writing through it should be visible on the next read.
  ReversedBlock rb{};
  rb.ids.path[0] = 0;
  dio_.set_reversed(rb, 1);

  ReversedBlock* ptr = dio_.get_reversed_pointer(1);
  ptr->ids.path[0] = 77; // direct write through pointer

  ReversedBlock* check = dio_.get_reversed_pointer(1);
  EXPECT_EQ(check->ids.path[0], 77u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Additional blocks
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, AdditionalBlockRoundTrip)
{
  AdditionalBlock ab{};
  ab.ids.path[0] = 3;
  ab.ids.path[1] = 7;
  ab.ids.additional[0] = 0;
  dio_.set_additional(ab, 1); // additional IDs are 1-indexed

  AdditionalBlock* got = dio_.get_additional_pointer(1);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(got->ids.path[0], 3u);
  EXPECT_EQ(got->ids.path[1], 7u);
  EXPECT_EQ(got->ids.additional[0], 0u);
}

TEST_F(DiskIOTest, AdditionalBlockAllPathSlots)
{
  AdditionalBlock ab{};
  for (uint16_t i = 0; i < ADDITIONAL_PATH_LINKS_AMOUNT; ++i) {
    ab.ids.path[i] = static_cast<PATH_ID_TYPE>(i + 5);
  }
  ab.ids.additional[0] = 0;
  dio_.set_additional(ab, 1);

  AdditionalBlock* got = dio_.get_additional_pointer(1);
  for (uint16_t i = 0; i < ADDITIONAL_PATH_LINKS_AMOUNT; ++i) {
    EXPECT_EQ(got->ids.path[i], static_cast<PATH_ID_TYPE>(i + 5))
      << "Path slot " << i << " wrong";
  }
}

TEST_F(DiskIOTest, AdditionalBlockChain)
{
  // Block 1 points to block 2
  AdditionalBlock ab1{};
  ab1.ids.path[0] = 10;
  ab1.ids.additional[0] = 2;
  dio_.set_additional(ab1, 1);

  AdditionalBlock ab2{};
  ab2.ids.path[0] = 20;
  ab2.ids.additional[0] = 0;
  dio_.set_additional(ab2, 2);

  AdditionalBlock* got1 = dio_.get_additional_pointer(1);
  EXPECT_EQ(got1->ids.additional[0], 2u);

  AdditionalBlock* got2 = dio_.get_additional_pointer(got1->ids.additional[0]);
  EXPECT_EQ(got2->ids.path[0], 20u);
  EXPECT_EQ(got2->ids.additional[0], 0u);
}

TEST_F(DiskIOTest, AdditionalPointerWritebackViaPtrAffectsDisk)
{
  AdditionalBlock ab{};
  ab.ids.path[0] = 0;
  dio_.set_additional(ab, 1);

  AdditionalBlock* ptr = dio_.get_additional_pointer(1);
  ptr->ids.path[0] = 99;

  AdditionalBlock* check = dio_.get_additional_pointer(1);
  EXPECT_EQ(check->ids.path[0], 99u);
}

// ─────────────────────────────────────────────────────────────────────────────
// copy_to_index / copy_from_index
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, CopyToIndexWritesCorrectBytes)
{
  // We'll use paths_count.index (index type 5) as the target.
  // Build a source mmap (we just write into the reversed mmap as a scratch
  // buffer via set_reversed to avoid creating an extra file).
  const uint32_t val = 0xDEADBEEF;
  // Write known bytes to a source buffer using paths_count setter
  dio_.set_path_count(1, val);

  // Now copy those 4 bytes from paths_count slot 1 → slot 2
  // source_pos = (id-1)*4 = 0, length = 4
  // target_pos = (id-1)*4 = 4
  // We do this by copying from mmap_paths_count into itself (different offsets)
  // The simplest approach: use a tiny external mmap_sink backed by another file.
  // Instead, just verify the setter/getter path above and test
  // copy_to_index with the reversed index as a staging area.

  // Create a small staging file to act as our source mmap_sink
  fs::path stage = dir_ / "stage.bin";
  create_file(stage, 4);
  std::error_code ec;
  mio::mmap_sink src = mio::make_mmap_sink(stage.string(), 0, mio::map_entire_file, ec);
  ASSERT_FALSE(ec);

  // Write our 4-byte pattern into the staging sink
  std::memcpy(&src[0], &val, 4);

  // Copy into paths_count slot 2 (offset 4)
  dio_.copy_to_index(5 /*paths_count*/, src, 4, 0, 4);
  src.unmap();

  EXPECT_EQ(dio_.get_path_count(2), val);
}

TEST_F(DiskIOTest, CopyFromIndexReadsCorrectBytes)
{
  const uint32_t val = 0xCAFEF00D;
  dio_.set_path_count(3, val);

  fs::path dest = dir_ / "dest.bin";
  create_file(dest, 4);
  std::error_code ec;
  mio::mmap_sink dst = mio::make_mmap_sink(dest.string(), 0, mio::map_entire_file, ec);
  ASSERT_FALSE(ec);

  // Copy from paths_count slot 3 (offset 8) into dst
  dio_.copy_from_index(5 /*paths_count*/, dst, 0, 8, 4);
  dst.sync(ec);

  uint32_t readback = 0;
  std::memcpy(&readback, &dst[0], 4);
  dst.unmap();

  EXPECT_EQ(readback, val);
}

// ─────────────────────────────────────────────────────────────────────────────
// shift_data
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, ShiftDataMovesBytes)
{
  // Use paths_count (index 5).  Write a known pattern at offset 0, shift
  // it forward by 4 bytes, verify it lands at offset 4.
  const uint32_t val = 0x12345678;
  dio_.set_path_count(1, val); // offset 0

  // shift: target_pos = 4, source_pos = 0, length = 4
  dio_.shift_data(5 /*paths_count*/, 4, 0, 4);

  EXPECT_EQ(dio_.get_path_count(2), val); // offset 4 = slot 2
}

// ─────────────────────────────────────────────────────────────────────────────
// sync_all
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, SyncAllSucceeds)
{
  // Write something to every file then sync — should not crash / return false
  dio_.set_path_count(1, 7u);
  std::vector<WordsFValue> wf(26);
  wf[0].location = 1;
  wf[0].id = 1;
  dio_.set_words_f(wf);
  EXPECT_TRUE(dio_.sync_all());
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence across unmap/remap
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(DiskIOTest, DataPersistsAcrossRemapping)
{
  // Write data, flush to disk, unmap, remap, verify data is still there.
  const uint32_t val = 0xABCD1234;
  dio_.set_path_count(1, val);
  dio_.sync_all();
  dio_.unmap();

  ASSERT_TRUE(dio_.map(dir_));
  EXPECT_EQ(dio_.get_path_count(1), val);
}

TEST_F(DiskIOTest, WordsFPersistsAcrossRemapping)
{
  std::vector<WordsFValue> wf(26);
  wf[5].location = 12345;
  wf[5].id = 99;
  dio_.set_words_f(wf);
  dio_.sync_all();
  dio_.unmap();

  ASSERT_TRUE(dio_.map(dir_));
  auto back = dio_.get_words_f();
  EXPECT_EQ(back[5].location, 12345u);
  EXPECT_EQ(back[5].id, 99u);
}

TEST_F(DiskIOTest, ReversedBlockPersistsAcrossRemapping)
{
  ReversedBlock rb{};
  rb.ids.path[0] = 42;
  dio_.set_reversed(rb, 0);
  dio_.sync_all();
  dio_.unmap();

  ASSERT_TRUE(dio_.map(dir_));
  ReversedBlock* got = dio_.get_reversed_pointer(0);
  EXPECT_EQ(got->ids.path[0], 42u);
}
