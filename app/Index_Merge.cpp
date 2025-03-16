#include "functions.h"
#include "lib/mio.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

void Index::add_reversed_to_word(index_combine_data &index_to_add,
                                 uint64_t &on_disk_count,
                                 std::vector<Transaction> &transactions,
                                 size_t &additional_new_needed_size,
                                 uint32_t &on_disk_id,
                                 const size_t &local_word_count,
                                 PathsMapping &paths_mapping) {

  // TODO: Overwrite path word count with inputet value. We need to pass the
  // path word count data from LocalIndex to here too first.

  if (on_disk_id * 10 + 10 > reversed_size) {
    log::error("Index: Reversed out of range. Index corrupted."); // index most
                                                                  // likely
                                                                  // corrupted.
  }

  size_t current_additional = 0;
  // we save all the slots that are free in here.
  std::vector<uint16_t> reversed_free;

  struct AdditionalFree {
    size_t additional_id;
    std::vector<uint16_t> free;
  };
  std::vector<AdditionalFree> additional_free;

  // load reversed block by reference.
  ReversedBlock *disk_reversed =
      reinterpret_cast<ReversedBlock *>(&mmap_reversed[on_disk_id * 10]);

  // we remove already existing path_ids from the local reversed and save slots
  // that are empty.
  for (int i = 0; i < 4; ++i) { // 4 blocks per reversed
    if (disk_reversed->ids[i] != 0) {
      index_to_add.words_and_reversed[local_word_count].reversed.erase(
          paths_mapping.by_disk[disk_reversed->ids[i]]);
    } else {
      reversed_free.push_back(i);
    }
  }

  // If there are additionals connected to reversed and there are still ids in
  // local reversed then we check each additional block for each item and remove
  // it from local until we either are out of local ids or there are no more
  // additionals connected.
  if (disk_reversed->ids[4] != 0 &&
      index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    current_additional = disk_reversed->ids[4];
    additional_free.push_back(
        {current_additional,
         {}}); // create an empty additional_free for the first one.

    AdditionalBlock *disk_additional = reinterpret_cast<AdditionalBlock *>(
        &mmap_additional[(current_additional - 1) * 50]);
    // load the current additional block. -1 because additional IDs start at 1.
    int i = 0;
    while (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
           0) { // Or when there is no new additional left it will break out
      // inside.
      if (i == 24) {
        if (disk_additional->ids[24] == 0) {
          // no additionals are left.
          break;
        } else {
          // load the new additional block
          current_additional = disk_additional->ids[24];
          disk_additional = reinterpret_cast<AdditionalBlock *>(
              &mmap_additional[(current_additional - 1) * 50]);
          i = 0;
          additional_free.push_back(
              {current_additional,
               {}}); // create an empty additional_free for the new one
        }
      }
      if (disk_additional->ids[i] == 0) {
        // save the free place in the current additional free. An empty one for
        // each additional is always created first.
        additional_free[additional_free.size() - 1].free.push_back(
            disk_additional->ids[i]);
      } else { // remove if it exists
        index_to_add.words_and_reversed[local_word_count].reversed.erase(
            paths_mapping.by_disk[disk_additional->ids[i]]);
      }

      ++i;
    }
  }

  // If there are still some left we will add them to the free slots.
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    // reversed free slots
    for (const uint16_t &free_slot : reversed_free) {
      Transaction reversed_add_transaction{
          0, 3, (on_disk_id * 10) + (free_slot * 2), 0, 1, 2};
      const auto &r_id =
          *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      // get the first local id. then convert it to disk id and save it to the
      // offset. PathOffset is the same as The 2 Byte value of a reversed slot.
      PathOffset content;
      content.offset = paths_mapping.by_local[r_id];
      // add it to the transaction to overwrite the empty place with the new
      // disk id and then delete it from the local reversed list.
      reversed_add_transaction.content = content.bytes[0] + content.bytes[1];
      index_to_add.words_and_reversed[local_word_count].reversed.erase(r_id);
      transactions.push_back(reversed_add_transaction);

      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0)
        break;
    }
    reversed_free.clear();
    for (const AdditionalFree &free_slot_block : additional_free) {
      for (const uint16_t &free_slot : free_slot_block.free) {
        Transaction additional_add_transaction{
            0, 4, ((free_slot_block.additional_id - 1) * 50) + (free_slot * 2),
            0, 1, 2};
        const auto &a_id =
            *index_to_add.words_and_reversed[local_word_count].reversed.begin();
        // get the first local id. then convert it to disk id and save it to the
        // offset. PathOffset is the same as The 2 Byte value of a reversed
        // slot.
        PathOffset content;
        content.offset = paths_mapping.by_local[a_id];
        // add it to the transaction to overwrite the empty place with the new
        // disk id and then delete it from the local reversed list.
        additional_add_transaction.content =
            content.bytes[0] + content.bytes[1];
        index_to_add.words_and_reversed[local_word_count].reversed.erase(a_id);
        transactions.push_back(additional_add_transaction);

        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
            0)
          break;
      }
    }
  }

  // if there are still some left we need to create new additionals.
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    Transaction additional_add_transaction;

    // If we need to append to a additional block or reversed
    if (current_additional != 0) {
      // Create a new additional transaction for the end
      additional_add_transaction = {
          0,
          4,
          (current_additional * 50) -
              2, // we overwrite the last additionals additional_id.
          0,
          1,
          2};
    } else { // reversed
      additional_add_transaction = {
          0, 3,
          (on_disk_id * 10) + 8, // we overwrite the reversed additional_id.
                                 // on_disk_id starts from 0.
          0, 1, 2};
    }

    PathOffset content;
    current_additional = ((additional_size + additional_new_needed_size) / 50) +
                         1; // get the ID of the new additional ID at the end.
    content.offset = current_additional;
    // add it to the transaction
    additional_add_transaction.content = content.bytes[0] + content.bytes[1];
    transactions.push_back(additional_add_transaction);

    // go through all missing local Ids and add them to additionals
    Transaction additional_new_transaction;
    additional_new_transaction.content.reserve(
        ((index_to_add.words_and_reversed[local_word_count].reversed.size() +
          19) /
         24) *
        50); // so many additional we need.
    int in_additional_counter = 0;
    for (int i = 0;
         i < index_to_add.words_and_reversed[local_word_count].reversed.size();
         ++i) {

      const auto &a_id =
          *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      // add path id for local id and then erase it.
      PathOffset content;
      content.offset = paths_mapping.by_local[a_id];
      additional_new_transaction.content += content.bytes[0] + content.bytes[1];
      index_to_add.words_and_reversed[local_word_count].reversed.erase(a_id);

      ++in_additional_counter;
      if (in_additional_counter ==
          24) { // if the current additional is full we add reference to the new
        // additional and go to the next.
        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
            0) { // If this will be the last one we will add 0.
          PathOffset add;
          add.offset = 0;
          additional_new_transaction.content += add.bytes[0] + add.bytes[1];
          in_additional_counter = 0;
          break;
        }
        in_additional_counter = 0;
        ++current_additional;
        PathOffset add;
        add.offset = current_additional;
        additional_new_transaction.content += add.bytes[0] + add.bytes[1];
      }
    }
    // If an additional is not full we fill it with 0.
    if (in_additional_counter != 0) {
      for (; in_additional_counter < 25; ++in_additional_counter) {
        PathOffset add;
        add.offset = 0;
        additional_new_transaction.content += add.bytes[0] + add.bytes[1];
      }
    }
    // add the transaction
    additional_new_transaction.header = {
        0,
        4,
        additional_size + additional_new_needed_size, // add to the end
        0,
        1,
        additional_new_transaction.content.length()};
    transactions.push_back(additional_new_transaction);
    additional_new_needed_size += additional_new_transaction.content.length();
  }

  // everything got checked, free slots filled and new additional if needed
  // created. it is done.
}

void Index::add_new_word(index_combine_data &index_to_add,
                         uint64_t &on_disk_count,
                         std::vector<Transaction> &transactions,
                         std::vector<Insertion> &words_insertions,
                         std::vector<Insertion> &reversed_insertions,
                         size_t &additional_new_needed_size,
                         size_t &words_new_needed_size,
                         size_t &reversed_new_needed_size, uint32_t &on_disk_id,
                         const size_t &local_word_count,
                         PathsMapping &paths_mapping) {

  // We create a insertion for the new word + word seperator at the start
  size_t word_length =
      index_to_add.words_and_reversed[local_word_count].word.length();
  Insertion new_word{
      on_disk_count,
      word_length +
          1}; // when we call it on_disk_count is before the word starts we just
  // compared and determined we went passed out target.
  new_word.content.reserve(word_length + 1);
  if (word_length + 30 > 254) {
    new_word.content += (char)255;
  } else {
    new_word.content += (char)(word_length + 30);
  }
  for (const char c : index_to_add.words_and_reversed[local_word_count].word) {
    new_word.content += c - 'a';
  }
  // update new needed size
  words_new_needed_size += word_length + 1;
  reversed_new_needed_size += 10; // always 10
  words_insertions.push_back(new_word);

  // We create a reversed insertion and remove the first 4 already and check if
  // needed more, if so we add already the next additional id to the reversed
  // insertion
  Insertion new_reversed{on_disk_id * 10, 10};
  new_reversed.content.reserve(10);
  ReversedBlock current_ReversedBlock{};
  for (int i = 0; i < 4; ++i) {
    if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
        0) {
      current_ReversedBlock.ids[i] = 0;
    } else {
      const auto &a_id =
          *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      current_ReversedBlock.ids[i] = paths_mapping.by_local[a_id];
      index_to_add.words_and_reversed[local_word_count].reversed.erase(a_id);
    }
  }
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() == 0) {
    current_ReversedBlock.ids[4] = 0;
  } else {
    size_t current_additional =
        ((additional_size + additional_new_needed_size) / 50) + 1; // 1-indexed
    current_ReversedBlock.ids[4] = current_additional;
    // we add all additionals using transactions and change additional new
    // needed size.

    Transaction new_additionals{};
    new_additionals.content.reserve(50);
    while (true) {
      AdditionalBlock additional{};
      for (int i = 0; i < 24; ++i) {
        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
            0) {
          additional.ids[i] = 0;
        } else {
          const auto &a_id = *index_to_add.words_and_reversed[local_word_count]
                                  .reversed.begin();
          additional.ids[i] = paths_mapping.by_local[a_id];
          index_to_add.words_and_reversed[local_word_count].reversed.erase(
              a_id);
        }
      }
      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0) {
        additional.ids[24] = 0;
      } else {
        additional.ids[24] = current_additional + 1;
      }

      for (int i = 0; i < 50; ++i) {
        new_additionals.content += additional.bytes[i];
      }

      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0)
        break; // if no more additional need to be added we break.
      ++current_additional;
    }
    new_additionals.header = {0,
                              4,
                              additional_size +
                                  additional_new_needed_size, // add to the end
                              0,
                              1,
                              new_additionals.content.length()};
    transactions.push_back(new_additionals);
    additional_new_needed_size += new_additionals.content.length();
  }

  // convert to bytes and add insertion
  for (int i = 0; i < 10; ++i) {
    new_reversed.content.push_back(current_ReversedBlock.bytes[i]);
  }
  reversed_insertions.push_back(new_reversed);
}

void Index::insertion_to_transactions(
    std::vector<Transaction> &transactions,
    std::vector<Insertion> &to_insertions,
    int index_type) { // index_type: 1 = words, 3 = reversed
  struct movements_temp_item {
    size_t start_pos;
    size_t end_pos;
    uint64_t byte_shift;
  };
  std::vector<movements_temp_item> movements_temp;

  size_t append_byte_shift = 0;

  size_t last_start_location = 0;
  size_t byte_shift = 0;
  // we make a list of moves so everything fits. then we make transactions from
  // last to first to move them.
  for (int i = 0; i < to_insertions.size(); ++i) {
    if (i == 0) {
      last_start_location = to_insertions[i].header.location;
    }
    if (to_insertions[i].header.location >=
        (index_type == 1 ? words_size - 1 : reversed_size - 1)) {
      // appendings. We will update their locations with the byte shift but not
      // move them around. The last check will also just move from the last non
      // append insertion till words/reversed_size
      to_insertions[i].header.location += byte_shift + append_byte_shift;
      append_byte_shift += to_insertions[i].header.content_length;

      continue;
    }
    if (last_start_location != to_insertions[i].header.location) {
      // if it's not the same it means it's a new block. WE add the current
      // block first.
      movements_temp.push_back(
          {last_start_location, to_insertions[i].header.location, byte_shift});
      last_start_location = to_insertions[i].header.location;
      to_insertions[i].header.location += byte_shift;
      byte_shift += to_insertions[i].header.content_length;

    } else {
      // It is the same. Update Byteshift only and apply byteshift.
      to_insertions[i].header.location += byte_shift;
      byte_shift += to_insertions[i].header.content_length;
    }
  }
  if (to_insertions.size() !=
      0) { // if its non zero it means there are insertions and the last one
           // didnt get added yet.
    movements_temp.push_back(
        {last_start_location,
         static_cast<size_t>(index_type == 1 ? words_size - 1
                                             : reversed_size - 1),
         byte_shift});
  }

  // now we create move transaction from last to first of the movements_temp.
  uint64_t backup_ids = 1;
  for (size_t i = movements_temp.size(); i-- > 0;) {
    size_t range = movements_temp[i].end_pos - movements_temp[i].start_pos;
    if (range > byte_shift) {
      // this means we copy over the data itself we are trying to move. Thats
      // why we create a backup of this before and then move it.
      Transaction create_backup{0,
                                static_cast<uint8_t>(index_type),
                                movements_temp[i].start_pos,
                                backup_ids,
                                3,
                                range};
      transactions.push_back(create_backup);
      Transaction move_operation{0,
                                 static_cast<uint8_t>(index_type),
                                 movements_temp[i].start_pos,
                                 backup_ids,
                                 0,
                                 range,
                                 ""};
      MoveOperationContent mov_content;
      mov_content.num = movements_temp[i].byte_shift;
      for (int j = 0; j < 8; ++j) {
        move_operation.content += mov_content.bytes[j];
      }
      transactions.push_back(move_operation);
      ++backup_ids;
      continue;
    }
    // with no backup
    Transaction move_operation{0,
                               static_cast<uint8_t>(index_type),
                               movements_temp[i].start_pos,
                               0,
                               0,
                               range,
                               ""};
    MoveOperationContent mov_content;
    mov_content.num = movements_temp[i].byte_shift;
    for (int j = 0; j < 8; ++j) {
      move_operation.content += mov_content.bytes[j];
    }
    transactions.push_back(move_operation);
  }
  movements_temp.clear();

  // now that there is place we write the insertions as normal at their place.
  // Their location already got updated so it's correct.
  for (int i = 0; i < to_insertions.size(); ++i) {
    Transaction insert_item{0,
                            static_cast<uint8_t>(index_type),
                            to_insertions[i].header.location,
                            0,
                            1,
                            to_insertions[i].header.content_length,
                            to_insertions[i].content};
    transactions.push_back(insert_item);
  }
  to_insertions.clear();
}

int Index::merge(index_combine_data &index_to_add) {
  log::write(2, "indexer: add: adding to existing index.");
  std::error_code ec;
  map();

  std::vector<Transaction> transactions;
  // for insertions: we will add to the list the locations current on disk, not
  // including already added ones. That will be done later. Additionals just
  // need + 50 on additional new needed size when added.
  std::vector<Insertion> words_insertions;
  std::vector<Insertion> reversed_insertions;
  size_t additional_new_needed_size = 0;
  size_t words_new_needed_size = 0;
  size_t reversed_new_needed_size = 0;

  size_t paths_l_size = index_to_add.paths.size();

  // So we can easily covert disk or local IDs fast.
  PathsMapping paths_mapping;

  // convert local paths to a map with value path and key id. We will go through
  // each path on disk and see if it is in the local index. Using a unordered
  // map we can check that very fast and get the local id so we can map the
  // local id to disk id and vica versa.
  // TODO: more memory efficent converting. Currently this would double the RAM
  // usage and violate the memory limit set by the user.
  std::unordered_map<std::string, uint16_t> paths_search;
  size_t path_insertion_count = 0;
  for (const std::string &s : index_to_add.paths) {
    paths_search[s] = path_insertion_count;
    ++path_insertion_count;
  }
  index_to_add.paths.clear();

  // go through index on disk and map disk Id to local Id.
  uint64_t on_disk_count = 0;
  uint32_t on_disk_id = 1;
  uint16_t next_path_end = 0; // if 0 the next 2 values are the header.

  // paths_size is the count of bytes of the index on disk.
  while (on_disk_count < paths_size) {
    if (on_disk_count + 1 <
        paths_size) { // we read 1 byte ahead for the offset to prevent
                      // accessing invalid data. The index format would allow it
                      // but it could be corrupted and not detected.
      PathOffset path_offset;
      // we read the offset so we know how long the path is and where the next
      // path starts.
      path_offset.bytes[0] = mmap_paths[on_disk_count];
      ++on_disk_count;
      path_offset.bytes[1] = mmap_paths[on_disk_count];
      next_path_end = path_offset.offset;
      ++on_disk_count;

      if (on_disk_count + next_path_end <
          paths_size + 1) { // check if we can read the whole path based on the
                            // offset.
        // refrence the path to a string and then search in the unordered map we
        // created earlier.
        std::string path_to_compare(&mmap_paths[on_disk_count], next_path_end);
        if (paths_search.find(path_to_compare) !=
            paths_search
                .end()) { // check if the disk path is found in memory index.

          paths_mapping.by_local[paths_search[path_to_compare]] =
              on_disk_id; // map disk id to memory id.
          paths_mapping.by_disk[on_disk_id] =
              paths_search[path_to_compare]; // map memory id to disk id.
          paths_search.erase(
              path_to_compare); // remove already found items so we know later
                                // which ones are not on disk.
          // check paths count and if it is different from the new one we create
          // a transaction and add it.
          PathsCountItem current_paths_count{};
          current_paths_count.bytes[0] = mmap_paths_count[(on_disk_id - 1) * 4];
          current_paths_count.bytes[1] =
              mmap_paths_count[((on_disk_id - 1) * 4) + 1];
          current_paths_count.bytes[2] =
              mmap_paths_count[((on_disk_id - 1) * 4) + 2];
          current_paths_count.bytes[3] =
              mmap_paths_count[((on_disk_id - 1) * 4) + 3];
          if (current_paths_count.num !=
              index_to_add.paths_count[paths_mapping.by_disk[on_disk_id]]) {
            // if it is different we create a transaction to correct it.
            std::string count_overwrite_content = "";
            PathsCountItem new_paths_count{
                index_to_add.paths_count[paths_mapping.by_disk[on_disk_id]]};
            count_overwrite_content.reserve(4);
            for (int i = 0; i < 4; ++i) {
              count_overwrite_content += new_paths_count.bytes[i];
            }
            Transaction count_to_overwrite_path_transaction{
                0,
                5,
                static_cast<uint64_t>((on_disk_id - 1) * 4),
                0,
                1,
                4,
                count_overwrite_content};
            transactions.push_back(count_to_overwrite_path_transaction);
          }
        }
        on_disk_count += next_path_end;
      } else {
        log::error("Index: Combine: invalid path content length. Aborting. "
                   "The Index could be corrupted.");
      }
      ++on_disk_id;

    } else {
      // Abort. A corrupted index would mess things up. If the corruption
      // could not get detected or fixed before here it is most likely broken.
      log::error("Index: Combine: invalid path content length indicator. "
                 "Aborting. The Index could be corrupted.");
    }
  }

  // go through all remaining paths_search elements, add a transaction and add
  // a new id to paths_mapping and create a resize transaction. also for paths
  // count.
  size_t paths_needed_space = 0;
  size_t count_needed_space = 0;
  std::string paths_add_content = "";
  std::string count_add_content = "";
  for (const auto &[key, value] : paths_search) {
    paths_mapping.by_local[value] = on_disk_id;
    paths_mapping.by_disk[on_disk_id] = value;
    // add all 2 byte offset + path to a string because all missing paths will
    // be added one after another
    PathOffset path_offset;
    path_offset.offset = key.length();
    paths_add_content += path_offset.bytes[0];
    paths_add_content += path_offset.bytes[1];
    paths_add_content += key;
    paths_needed_space += key.length() + 2;

    // add to paths count
    PathsCountItem paths_count_current{};
    paths_count_current.num = index_to_add.paths_count[value];
    for (int i = 0; i < 4; ++i) {
      count_add_content += paths_count_current.bytes[i];
    }
    count_needed_space += 4;
    ++on_disk_id;
  }
  // write it at the end at once if needed.
  if (paths_needed_space != 0) {
    // resize so all paths + offset fit.
    Transaction resize_transaction{0, 0, 0,
                                   0, 2, paths_size + paths_needed_space};
    transactions.push_back(resize_transaction);
    // write it to the now free space at the end of the file.
    Transaction to_add_path_transaction{0,
                                        0,
                                        static_cast<uint64_t>(paths_size),
                                        0,
                                        1,
                                        paths_needed_space,
                                        paths_add_content};
    transactions.push_back(to_add_path_transaction);

    // resize so all paths counts fit.
    Transaction count_resize_transaction{
        0, 5, 0, 0, 2, paths_count_size + count_needed_space};
    transactions.push_back(count_resize_transaction);
    // write it to the now free space at the end of the file.
    Transaction count_to_add_path_transaction{
        0,
        5,
        static_cast<uint64_t>(paths_count_size),
        0,
        1,
        count_needed_space,
        count_add_content};
    transactions.push_back(count_to_add_path_transaction);
  }
  paths_add_content = "";
  count_add_content = "";
  paths_search.clear();

  // copy words_f into memory
  std::vector<WordsFValue> words_f(26);
  for (int i = 0; i < 26; ++i) {
    std::memcpy(&words_f[i].bytes[0], &mmap_words_f[i * 12], 12);
  }

  // This is needed when we add new words. Each time we add a new word the start
  // of all following chars will change by how much we add. Here we will note
  // down how many now bytes got added at the next char. E.g new word at 'f'. We
  // add 5 (word is 4 letters + 1 seperator) to the location of 'g' because its
  // the next occurence. After all words got added we will update the real
  // words_f by combining the current with all the ones that came before. e.g a
  // has 2, b has 5, c has 9. wordsf for b adds 5+2. c = 9+5+2. We have 27
  // because when adding new words we add to + 1 and when its z + 1 its invalid.
  // we will just ignore it
  std::vector<uint64_t> words_F_change(27, 0);

  // local index words needs to have atleast 1 value. Is checked by LocalIndex
  // add_to_disk.
  on_disk_count = 0;
  on_disk_id = 0;
  size_t local_word_count = 0;
  size_t local_word_length = index_to_add.words_and_reversed[0].word.length();
  char disk_first_char = 'a';
  char local_first_char =
      index_to_add.words_and_reversed[local_word_count].word[0];

  uint64_t disk_additional_ids = (additional_size / 50) + 1;

  // check each word on disk. if it is different first letter we will skip using
  // words_f. we will compare chars to chars until they differ. we then know if
  // it is coming first or we are passed it and need to insert the word or we
  // are at the same word.

  while (on_disk_count < words_size) {
    // If the current word first char is different we use words_f to set the
    // location to the start of that char.
    if (disk_first_char < local_first_char) {
      if (words_f[local_first_char - 'a'].location < words_size) {
        log::write(1, "Index: Merge: Skipping using Words_f Table");
        on_disk_count = words_f[local_first_char - 'a'].location;
        on_disk_id = words_f[local_first_char - 'a'].id;
        disk_first_char = local_first_char;
      } else {
        // This should not happen. Index is corrupted.
        log::error("Index: Combine: Words_f char value is higher than words "
                   "index size. This means the index is corrupted. Reset the "
                   "index and report this problem.");
      }
    }

    // read the one byte word sperator.
    uint8_t word_seperator = mmap_words[on_disk_count];

    if (word_seperator < 31 ||
        (word_seperator - 29) + on_disk_count >
            words_size + 1) { // 0-30 is reserved. if it is higher it is for
      // seperator. If the seperator here is 0-30 the index
      // is corrupted.
      log::error(
          "Index: Combine: word seperator is invalid. This means the index is "
          "most likely corrupted. Stopping to protect the index.");
    }
    if (disk_first_char - 'a' <
        mmap_words[on_disk_count + 1]) { // + 1 because of the word seperator
      disk_first_char = mmap_words[on_disk_count + 1] + 'a';
    }
    if (word_seperator ==
        255) { // This means the word is larger than 255 bytes. We need to count
               // it manually until we reach another 30< byte.
      // go through start position until end position comparing each char until
      // either it is smaller or bigger. then just try to find the next
      // seperator. update word_lentgh and count and first char.
      continue;
    }

    for (int i = 0; i < word_seperator - 30; ++i) {
      // If current chars are the same + word on disk length same as on local
      // length and last char add to existing word
      if ((int)mmap_words[on_disk_count + 1 + i] ==
          (int)(index_to_add.words_and_reversed[local_word_count].word[i] -
                'a')) {
        // If its last char and words are the same length we found it.
        if (i == local_word_length - 1 &&
            word_seperator - 30 == local_word_length) {
          // add to existing
          log::write(2, "Index: Merge: Found existing word");
          add_reversed_to_word(index_to_add, on_disk_count, transactions,
                               additional_new_needed_size, on_disk_id,
                               local_word_count, paths_mapping);
          ++local_word_count;
          if (local_word_count ==
              index_to_add.words_and_reversed
                  .size()) { // if not more words to compare quit.
            break;
          }
          local_word_length =
              index_to_add.words_and_reversed[local_word_count].word.length();
          on_disk_count +=
              word_seperator -
              29; // 29 because its length of word + then the next seperator
          ++on_disk_id;
          local_first_char =
              index_to_add.words_and_reversed[local_word_count].word[0];

          break;
        }
        // If its last char and local word is at the end and not same length
        // means we need to insert before
        if (i == local_word_length - 1) {
          // insert new
          log::write(1, "Index: Merge: Add new Word");
          add_new_word(index_to_add, on_disk_count, transactions,
                       words_insertions, reversed_insertions,
                       additional_new_needed_size, words_new_needed_size,
                       reversed_new_needed_size, on_disk_id, local_word_count,
                       paths_mapping);
          words_F_change[disk_first_char - 'a' + 1] += local_word_length + 1;
          ++local_word_count;
          if (local_word_count ==
              index_to_add.words_and_reversed
                  .size()) { // if not more words to compare quit.
            break;
          }
          local_word_length =
              index_to_add.words_and_reversed[local_word_count].word.length();
          local_first_char =
              index_to_add.words_and_reversed[local_word_count].word[0];
          break;
        }

        // If its the last on disk char and at the end and not the same
        // length. means we need to skip this word.
        if (i == word_seperator - 31) {
          // skip
          log::write(1, "Index: Merge: Skip Word on Disk");
          on_disk_count +=
              word_seperator -
              29; // 29 because its length of word + then the next seperator
          ++on_disk_id;
          break;
        }
      }

      // If disk char > local char
      if ((int)mmap_words[on_disk_count + 1 + i] >
          (int)(index_to_add.words_and_reversed[local_word_count].word[i] -
                'a')) {
        // insert new
        log::write(1, "Index: Merge: Add new Word");
        add_new_word(index_to_add, on_disk_count, transactions,
                     words_insertions, reversed_insertions,
                     additional_new_needed_size, words_new_needed_size,
                     reversed_new_needed_size, on_disk_id, local_word_count,
                     paths_mapping);
        words_F_change[disk_first_char - 'a' + 1] += local_word_length + 1;
        ++local_word_count;
        if (local_word_count ==
            index_to_add.words_and_reversed
                .size()) { // if not more words to compare quit.
          break;
        }
        local_word_length =
            index_to_add.words_and_reversed[local_word_count].word.length();
        local_first_char =
            index_to_add.words_and_reversed[local_word_count].word[0];
        break;
      }

      // If disk char < local char
      if ((int)mmap_words[on_disk_count + 1 + i] <
          (int)(index_to_add.words_and_reversed[local_word_count].word[i] -
                'a')) {
        // skip
        log::write(1, "Index: Merge: Skip Word on Disk");
        on_disk_count +=
            word_seperator -
            29; // 29 because its length of word + then the next seperator
        ++on_disk_id;
        break;
      }
    }
    if (local_word_count == index_to_add.words_and_reversed
                                .size()) { // if not more words to compare quit.
      break;
    }
  }

  // we need to insert all words that came after the last word on disk.
  // Update words_f_change too.

  for (; local_word_count < index_to_add.words_and_reversed.size();
       ++local_word_count) {
    local_word_length =
        index_to_add.words_and_reversed[local_word_count].word.length();
    // Even tho we add multiple words at the same on_disk_id or
    // on_disk_count it doesnt matter because when insertions are
    // processed it will add all the newly added word count to the
    // insertion location.
    log::write(2, "Index: Merge: Adding a new word at the end");
    add_new_word(index_to_add, on_disk_count, transactions, words_insertions,
                 reversed_insertions, additional_new_needed_size,
                 words_new_needed_size, reversed_new_needed_size, on_disk_id,
                 local_word_count, paths_mapping);
    words_F_change[(index_to_add.words_and_reversed[local_word_count].word[0] -
                    'a') +
                   1] += local_word_length + 1;
  }

  // calculate the new words f values and create a transaction to update
  // all. This is not an ideal implementation because we copy the whole
  // thing. When we add custom words_f length then we can make a better
  // implementation that is faster and saves memory and space.
  Transaction words_f_new{0, 2, 0, 0, 1, 312};
  words_f_new.content.resize(312);
  uint64_t all_size = 0;
  for (int i = 0; i < 26; ++i) {
    all_size += words_F_change[i];
    words_f[i].location += all_size;
    std::memcpy(&words_f_new.content[i * 12], &words_f[i].bytes[0], 12);
  }
  transactions.push_back(words_f_new);

  words_f_new.content.clear(); // free some memory.

  // Now we have figured out everything we want to do. Now we need to
  // finish the Transaction List and then write it to disk. First add a
  // resize Transaction for words, reversed and additional at the start of
  // the List.
  log::write(2, "Index: Creating resize transaction for words with size: " +
                    std::to_string(words_size + words_new_needed_size));
  log::write(2, "Index: Creating resize transaction for reversed with size: " +
                    std::to_string(reversed_size + reversed_new_needed_size));
  log::write(2,
             "Index: Creating resize transaction for additional with size: " +
                 std::to_string(additional_size + additional_new_needed_size));
  Transaction resize_words{0, 1, 0, 0, 2, words_size + words_new_needed_size,
                           ""};
  transactions.insert(transactions.begin(), resize_words);
  Transaction resize_reversed{
      0, 3, 0, 0, 2, reversed_size + reversed_new_needed_size, ""};
  transactions.insert(transactions.begin(), resize_reversed);
  Transaction resize_additional{
      0, 4, 0, 0, 2, additional_size + additional_new_needed_size, ""};
  transactions.insert(transactions.begin(), resize_additional);

  // Now we need to convert the Insertion to Transactions.
  // First we need to make Transactions to make place for the insertion.
  // We have already resized so there is enough space for it. We need to
  // create the Transaction so data only gets moved once.
  insertion_to_transactions(transactions, words_insertions, 1);
  insertion_to_transactions(transactions, reversed_insertions, 3);

  // Now we need to write the Transaction List to disk.
  // The Transactions are saved in indexpath / transaction /
  // transaction.list Backups are saved in indexpath / transaction /
  // backups / backupid.backup

  std::filesystem::path transaction_path =
      index_path / "transaction" / "transaction.list";

  size_t recalculated_size = 0;
  for (const auto &tx : transactions) {
    recalculated_size += 27; // Header size
    if (tx.header.operation_type != 2 && tx.header.operation_type != 3) {
      if (tx.header.operation_type == 0) {
        recalculated_size += 8; // Move operations need 8 bytes
      } else {
        recalculated_size += tx.header.content_length;
      }
    }
  }

  // just create an empty file which we then resize to the required size
  // and fill with mmap to keep consistency with the other file operations
  // on disks.
  std::ofstream{transaction_path};
  resize(transaction_path, recalculated_size);

  mio::mmap_sink mmap_transactions;
  mmap_transactions = mio::make_mmap_sink((transaction_path).string(), 0,
                                          mio::map_entire_file, ec);

  size_t transaction_file_location = 0;
  for (size_t i = 0; i < transactions.size(); ++i) {
    // copy the header first. Then check the operation type and then copy
    // the content if needed.
    std::memcpy(&mmap_transactions[transaction_file_location],
                &transactions[i].header.bytes[0], 27);
    transaction_file_location += 27;
    if (transactions[i].header.operation_type != 2 &&
        transactions[i].header.operation_type != 3) { // resize or backup
      if (transactions[i].header.operation_type == 0) {
        // For move operations content is 8 bytes long as a uint64_t to
        // represent the byte shift. content length is used for end pos.
        std::memcpy(&mmap_transactions[transaction_file_location],
                    &transactions[i].content[0], 8);
        transaction_file_location += 8;
      } else {
        std::memcpy(&mmap_transactions[transaction_file_location],
                    &transactions[i].content[0],
                    transactions[i].header.content_length);
        transaction_file_location += transactions[i].header.content_length;
      }
    }
  }
  // Transaction List written. unmap and free memory.
  mmap_transactions.sync(ec);
  // mark first as started to signal that writing was successful.
  mmap_transactions[0] = 1; // first item status to 1
  mmap_transactions.sync(ec);
  mmap_transactions.unmap();

  transactions.clear();
  index_to_add.paths.clear();
  index_to_add.paths_count.clear();
  index_to_add.words_and_reversed.clear();

  if (ec)
    return 1;
  return 0;
}
