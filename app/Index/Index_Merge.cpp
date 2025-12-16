#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <cstring>
#include <filesystem>
#include <string>

void Index::add_reversed_to_word(
  index_combine_data& index_to_add, uint64_t& on_disk_count,
  std::vector<Transaction>& transactions, size_t& additional_new_needed_size,
  size_t& reversed_new_needed_size, uint64_t& on_disk_id,
  const size_t& local_word_count, PathsMapping& paths_mapping)
{
  if (on_disk_id * REVERSED_ENTRY_SIZE + REVERSED_ENTRY_SIZE > disk_io.get_reversed_size()) {
    Log::error("Index: Reversed out of range. Index corrupted."); // index most
    // likely
    // corrupted.
  }

  ADDITIONAL_ID_TYPE current_additional = 0;
  // we save all the slots that are free in here.
  std::vector<uint16_t> reversed_free;

  struct AdditionalFree
  {
    ADDITIONAL_ID_TYPE additional_id;
    std::vector<uint16_t> free;
  };
  std::vector<AdditionalFree> additional_free;

  // load reversed block by reference.
  ReversedBlock* disk_reversed = disk_io.get_reversed_pointer(on_disk_id);

  // we remove already existing path_ids from the local reversed and save slots
  // that are empty.
  for (uint16_t i = 0; i < REVERSED_PATH_LINKS_AMOUNT;
       ++i) {
    // 4 blocks per reversed
    if (disk_reversed->ids.path[i] != 0) {
      index_to_add.words_and_reversed[local_word_count].reversed.erase(
        paths_mapping.by_disk[disk_reversed->ids.path[i]]);
    }
    else {
      reversed_free.push_back(i);
    }
  }

  // If there are additionals connected to reversed and there are still ids in
  // local reversed then we check each additional block for each item and remove
  // it from local until we either are out of local ids or there are no more
  // additionals connected.
  if (disk_reversed->ids.additional[0] != 0 &&
    index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    current_additional = disk_reversed->ids.additional[0];
    additional_free.push_back(
      {
        current_additional,
        {}
      }); // create an empty additional_free for the first one.
    if ((current_additional * ADDITIONAL_ENTRY_SIZE) > disk_io.get_additional_size()) {
      Log::error("Index: path_ids_from_word_id: to search word id would be at "
        "nonexisting location. Index most likely corrupt. Exiting");
    }
    AdditionalBlock* disk_additional = disk_io.get_additional_pointer(current_additional);
    // load the current additional block. -1 because additional IDs start at 1.
    size_t i = 0;
    while (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
      0) {
      // Or when there is no new additional left it will break out
      // inside.
      if (i == ADDITIONAL_PATH_LINKS_AMOUNT) {
        if (disk_additional->ids.additional[0] == 0) {
          // no additionals are left.
          break;
        }
        else {
          // load the new additional block
          current_additional = disk_additional->ids.additional[0];
          if ((current_additional * ADDITIONAL_ENTRY_SIZE) > disk_io.get_additional_size()) {
            Log::error(
              "Index: path_ids_from_word_id: to search word id would be at "
              "nonexisting location. Index most likely corrupt. Exiting");
          }
          disk_additional = disk_io.get_additional_pointer(current_additional);
          i = 0;
          additional_free.push_back(
            {
              current_additional,
              {}
            }); // create an empty additional_free for the new one
        }
      }
      if (disk_additional->ids.path[i] == 0) {
        // save the free place in the current additional free. An empty one for
        // each additional is always created first.
        additional_free[additional_free.size() - 1].free.push_back(i);
      }
      else {
        // remove if it exists
        index_to_add.words_and_reversed[local_word_count].reversed.erase(
          paths_mapping.by_disk[disk_additional->ids.path[i]]);
      }

      ++i;
    }
  }

  // If there are still some left we will add them to the free slots.
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    // reversed free slots
    for (const uint16_t& free_slot : reversed_free) {
      const auto& r_id = *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      const PATH_ID_TYPE disk_id = paths_mapping.by_local[r_id];

      Transaction reversed_add_transaction{
        {
          {
            0,
            3,
            (on_disk_id * REVERSED_ENTRY_SIZE) +
            reversed_new_needed_size +
            (free_slot * PATH_ID_LINK_SIZE),
            0,
            1,
            PATH_ID_LINK_SIZE
          }
        },
        {reinterpret_cast<const char*>(&disk_id), PATH_ID_LINK_SIZE}
      };

      index_to_add.words_and_reversed[local_word_count].reversed.erase(r_id);
      transactions.push_back(reversed_add_transaction);

      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
        0)
        break;
    }
    if (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
      0) {
      reversed_free.clear();
      for (const AdditionalFree& free_slot_block : additional_free) {
        for (const uint16_t& free_slot : free_slot_block.free) {
          const auto& a_id = *index_to_add.words_and_reversed[local_word_count]
                              .reversed.begin();
          const PATH_ID_TYPE disk_id = paths_mapping.by_local[a_id];


          Transaction additional_add_transaction{
            {
              {
                0,
                4,
                ((free_slot_block.additional_id - 1) * ADDITIONAL_ENTRY_SIZE) +
                (free_slot * PATH_ID_LINK_SIZE),
                0,
                1,
                PATH_ID_LINK_SIZE
              }
            },
            {reinterpret_cast<const char*>(&disk_id), PATH_ID_LINK_SIZE}
          };

          index_to_add.words_and_reversed[local_word_count].reversed.erase(
            a_id);
          transactions.push_back(additional_add_transaction);

          if (index_to_add.words_and_reversed[local_word_count]
              .reversed.size() == 0)
            break;
        }
        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0)
          break;
      }
    }
  }

  // if there are still some left we need to create new additionals.
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() != 0) {
    Transaction additional_add_transaction;

    // If we need to append to an additional block or reversed
    if (current_additional != 0) {
      // Create a new additional transaction for the end
      additional_add_transaction = {
        0, 4,
        (current_additional * ADDITIONAL_ENTRY_SIZE) -
        ADDITIONAL_ID_LINK_SIZE, // we overwrite the last additionals
        // additional_id.
        0, 1, ADDITIONAL_ID_LINK_SIZE
      };
    }
    else {
      // reversed
      additional_add_transaction = {
        0, 3,
        (on_disk_id * REVERSED_ENTRY_SIZE) + reversed_new_needed_size +
        REVERSED_ENTRY_SIZE -
        ADDITIONAL_ID_LINK_SIZE, // we overwrite the reversed
        // additional_id. on_disk_id starts from
        // 0.
        0, 1, ADDITIONAL_ID_LINK_SIZE
      };
    }

    AdditionalOffset content;
    current_additional = ((disk_io.get_additional_size() + additional_new_needed_size) /
        ADDITIONAL_ENTRY_SIZE) +
      1; // get the ID of the new additional ID at the end.
    content.offset = current_additional;
    // add it to the transaction
    for (uint8_t i = 0; i < ADDITIONAL_ID_LINK_SIZE; ++i) {
      additional_add_transaction.content += content.bytes[i];
    }
    transactions.push_back(additional_add_transaction);

    // go through all missing local Ids and add them to additionals
    Transaction additional_new_transaction;
    additional_new_transaction.content.reserve(
      ((index_to_add.words_and_reversed[local_word_count].reversed.size() +
          ADDITIONAL_PATH_LINKS_AMOUNT - 1) /
        ADDITIONAL_PATH_LINKS_AMOUNT) *
      ADDITIONAL_ENTRY_SIZE); // so many additional we need.
    size_t in_additional_counter = 0;
    while (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
      0) {
      const auto& a_id =
        *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      // add path id for local id and then erase it.
      PathIDOffset content;
      content.offset = paths_mapping.by_local[a_id];
      for (uint8_t i = 0; i < PATH_ID_LINK_SIZE; ++i) {
        additional_new_transaction.content += content.bytes[i];
      }
      index_to_add.words_and_reversed[local_word_count].reversed.erase(a_id);

      ++in_additional_counter;
      if (in_additional_counter ==
        ADDITIONAL_PATH_LINKS_AMOUNT) {
        // if the current additional is full
        // we add reference to the new
        // additional and go to the next.
        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0) {
          // If this will be the last one we will add 0.
          AdditionalOffset add;
          add.offset = 0;
          for (uint8_t i = 0; i < ADDITIONAL_ID_LINK_SIZE; ++i) {
            additional_new_transaction.content += add.bytes[i];
          }
          in_additional_counter = 0;
          break;
        }
        in_additional_counter = 0;
        ++current_additional;
        AdditionalOffset add;
        add.offset = current_additional;
        for (uint8_t i = 0; i < ADDITIONAL_ID_LINK_SIZE; ++i) {
          additional_new_transaction.content += add.bytes[i];
        };
      }
    }
    // If an additional is not full we fill it with 0.
    if (in_additional_counter != 0) {
      for (; in_additional_counter < ADDITIONAL_PATH_LINKS_AMOUNT;
             ++in_additional_counter) {
        PathIDOffset add;
        add.offset = 0;
        for (uint8_t i = 0; i < PATH_ID_LINK_SIZE; ++i) {
          additional_new_transaction.content += add.bytes[i];
        }
      }
      AdditionalOffset add_additional;
      add_additional.offset = 0;
      for (uint8_t i = 0; i < ADDITIONAL_ID_LINK_SIZE; ++i) {
        additional_new_transaction.content += add_additional.bytes[i];
      }
    }
    // add the transaction
    additional_new_transaction.header = {
      0,
      4,
      disk_io.get_additional_size() + additional_new_needed_size, // add to the end
      0,
      1,
      additional_new_transaction.content.length()
    };
    transactions.push_back(additional_new_transaction);
    additional_new_needed_size += additional_new_transaction.content.length();
  }

  // everything got checked, free slots filled and new additional if needed
  // created. it is done.
}

void Index::add_new_word(index_combine_data& index_to_add,
                         uint64_t& on_disk_count,
                         std::vector<Transaction>& transactions,
                         std::vector<Insertion>& words_insertions,
                         std::vector<Insertion>& reversed_insertions,
                         size_t& additional_new_needed_size,
                         size_t& words_new_needed_size,
                         size_t& reversed_new_needed_size, uint64_t& on_disk_id,
                         const size_t& local_word_count,
                         PathsMapping& paths_mapping)
{
  // We create an insertion for the new word + word seperator at the start
  size_t word_length =
    index_to_add.words_and_reversed[local_word_count].word.length();
  Insertion new_word{
    on_disk_count,
    word_length +
    1
  }; // when we call it on_disk_count is before the word starts we just
  // compared and determined we went passed our target.
  new_word.content.reserve(word_length + WORD_SEPARATOR_SIZE);
  WordSeperator word_seperator;
  word_seperator.seperator = word_length;

  for (uint8_t i = 0; i < WORD_SEPARATOR_SIZE; ++i) {
    new_word.content += word_seperator.bytes[i];
  }
  for (const char c : index_to_add.words_and_reversed[local_word_count].word) {
    new_word.content += c;
  }
  // update new needed size
  words_new_needed_size += word_length + WORD_SEPARATOR_SIZE;
  words_insertions.push_back(new_word);

  // We create a reversed insertion and remove the first 4 already and check if
  // needed more, if so we add already the next additional id to the reversed
  // insertion
  Insertion new_reversed{on_disk_id * REVERSED_ENTRY_SIZE, REVERSED_ENTRY_SIZE};
  reversed_new_needed_size += REVERSED_ENTRY_SIZE;
  new_reversed.content.reserve(REVERSED_ENTRY_SIZE);
  ReversedBlock current_ReversedBlock{};
  for (uint16_t i = 0; i < REVERSED_PATH_LINKS_AMOUNT; ++i) {
    if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
      0) {
      current_ReversedBlock.ids.path[i] = 0;
    }
    else {
      const auto& a_id =
        *index_to_add.words_and_reversed[local_word_count].reversed.begin();
      current_ReversedBlock.ids.path[i] = paths_mapping.by_local[a_id];
      index_to_add.words_and_reversed[local_word_count].reversed.erase(a_id);
    }
  }
  if (index_to_add.words_and_reversed[local_word_count].reversed.size() == 0) {
    current_ReversedBlock.ids.additional[0] = 0;
  }
  else {
    ADDITIONAL_ID_TYPE current_additional =
      ((disk_io.get_additional_size() + additional_new_needed_size) /
        ADDITIONAL_ENTRY_SIZE) +
      1; // 1-indexed
    current_ReversedBlock.ids.additional[0] = current_additional;
    // we add all additionals using transactions and change additional new
    // needed size.

    Transaction new_additionals{};
    new_additionals.content.reserve(ADDITIONAL_ENTRY_SIZE);
    while (true) {
      AdditionalBlock additional{};
      for (int i = 0; i < ADDITIONAL_PATH_LINKS_AMOUNT; ++i) {
        if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
          0) {
          additional.ids.path[i] = 0;
        }
        else {
          const auto& a_id = *index_to_add.words_and_reversed[local_word_count]
                              .reversed.begin();
          additional.ids.path[i] = paths_mapping.by_local[a_id];
          index_to_add.words_and_reversed[local_word_count].reversed.erase(
            a_id);
        }
      }
      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
        0) {
        additional.ids.additional[0] = 0;
      }
      else {
        additional.ids.additional[0] = current_additional + 1;
      }

      for (int i = 0; i < ADDITIONAL_ENTRY_SIZE; ++i) {
        new_additionals.content += additional.bytes[i];
      }

      if (index_to_add.words_and_reversed[local_word_count].reversed.size() ==
        0)
        break; // if no more additional need to be added we break.
      ++current_additional;
    }
    new_additionals.header = {
      0,
      4,
      disk_io.get_additional_size() +
      additional_new_needed_size, // add to the end
      0,
      1,
      new_additionals.content.length()
    };
    transactions.push_back(new_additionals);
    additional_new_needed_size += new_additionals.content.length();
  }

  // convert to bytes and add insertion
  for (size_t i = 0; i < REVERSED_ENTRY_SIZE; ++i) {
    new_reversed.content.push_back(current_ReversedBlock.bytes[i]);
  }
  reversed_insertions.push_back(new_reversed);
}

void Index::insertion_to_transactions(
  std::vector<Transaction>& transactions,
  std::vector<Transaction>& move_transactions,
  std::vector<Insertion>& to_insertions,
  int index_type)
{
  // index_type: 1 = words, 3 = reversed
  if (to_insertions.size() == 0)
    return;
  struct movements_temp_item
  {
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
  for (size_t i = 0; i < to_insertions.size(); ++i) {
    if (i == 0) {
      last_start_location = to_insertions[i].header.location;
    }
    if (to_insertions[i].header.location >=
      (index_type == 1 ? disk_io.get_words_size() - 1 : disk_io.get_reversed_size() - 1)) {
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
    }
    else {
      // It is the same. Update Byteshift only and apply byteshift.
      to_insertions[i].header.location += byte_shift;
      byte_shift += to_insertions[i].header.content_length;
    }
  }
  if (to_insertions.size() !=
    0) {
    // if its non-zero it means there are insertions and the last one
    // didn't get added yet.
    movements_temp.push_back(
      {
        last_start_location,
        static_cast<size_t>(index_type == 1 ? disk_io.get_words_size() : disk_io.get_reversed_size()),
        byte_shift
      });
  }

  // now we create move transaction from last to first of the movements_temp.
  uint64_t backup_ids = 1;
  for (size_t i = movements_temp.size(); i-- > 0;) {
    if (movements_temp.size() == 0)
      break;
    size_t range = movements_temp[i].end_pos - movements_temp[i].start_pos;
    if (range > byte_shift) {
      // this means we copy over the data itself we are trying to move. That's
      // why we create a backup of this before and then move it.
      Transaction create_backup{
        0,
        static_cast<uint8_t>(index_type),
        movements_temp[i].start_pos,
        backup_ids,
        3,
        range
      };
      move_transactions.push_back(create_backup);
      Transaction move_operation{
        0,
        static_cast<uint8_t>(index_type),
        movements_temp[i].start_pos,
        backup_ids,
        0,
        range,
        ""
      };
      MoveOperationContent mov_content;
      mov_content.num = movements_temp[i].byte_shift;
      for (int j = 0; j < 8; ++j) {
        move_operation.content += mov_content.bytes[j];
      }
      move_transactions.push_back(move_operation);
      ++backup_ids;
      continue;
    }
    // with no backup
    Transaction move_operation{
      0,
      static_cast<uint8_t>(index_type),
      movements_temp[i].start_pos,
      0,
      0,
      range,
      ""
    };
    MoveOperationContent mov_content;
    mov_content.num = movements_temp[i].byte_shift;
    for (int j = 0; j < 8; ++j) {
      move_operation.content += mov_content.bytes[j];
    }
    move_transactions.push_back(move_operation);
  }
  movements_temp.clear();

  // now that there is place we write the insertions as normal at their place.
  // Their location already got updated so it's correct.
  for (size_t i = 0; i < to_insertions.size(); ++i) {
    Transaction insert_item{
      0,
      static_cast<uint8_t>(index_type),
      to_insertions[i].header.location,
      0,
      1,
      to_insertions[i].header.content_length,
      to_insertions[i].content
    };
    transactions.push_back(insert_item);
  }
  to_insertions.clear();
}

int Index::merge(index_combine_data& index_to_add)
{
  Log::write(1, "indexer: add: adding to existing index.");
  std::error_code ec;
  map();

  std::vector<Transaction> transactions;
  std::vector<Transaction> move_transactions;
  // for insertions: we will add to the list the locations current on disk, not
  // including already added ones. That will be done later. Additionals just
  // need + ADDITIONAL_ENTRY_SIZE on additional new needed size when
  // added.
  std::vector<Insertion> words_insertions;
  std::vector<Insertion> reversed_insertions;
  size_t additional_new_needed_size = 0;
  size_t words_new_needed_size = 0;
  size_t reversed_new_needed_size = 0;

  // So we can easily convert disk or local IDs fast.
  PathsMapping paths_mapping;

  // convert local paths to a map with value path and key id. We will go through
  // each path on disk and see if it is in the local index. Using a unordered
  // map we can check that very fast and get the local id so we can map the
  // local id to disk id and vica versa.
  // TODO: more memory efficent converting. Currently this would double the RAM
  // usage and violate the memory limit set by the user.
  std::unordered_map<std::string, PATH_ID_TYPE> paths_search;
  size_t path_insertion_count = 0;
  for (const std::string& s : index_to_add.paths) {
    paths_search[s] = path_insertion_count;
    ++path_insertion_count;
  }
  index_to_add.paths.clear();

  // go through index on disk and map disk ID to local ID.
  uint64_t on_disk_count = 0;
  uint64_t on_disk_id = 1;
  uint16_t next_path_end = 0; // if 0 the next 2 values are the header.

  // paths_size is the count of bytes of the index on disk.
  while (on_disk_count < disk_io.get_paths_size()) {
    if (on_disk_count + 1 <
      disk_io.get_paths_size()) {
      // we read 1 byte ahead for the offset to prevent
      // accessing invalid data. The index format would allow it,
      // but it could be corrupted and not detected.
      next_path_end = disk_io.get_path_separator(on_disk_count);
      on_disk_count += 2;

      if (on_disk_count + next_path_end <
        disk_io.get_paths_size() + 1) {
        // check if we can read the whole path based on the
        // offset.
        // refrence the path to a string and then search in the unordered map we
        // created earlier.
        std::string path_to_compare = disk_io.get_path(on_disk_count, next_path_end);
        if (paths_search.contains(path_to_compare)) {
          // check if the disk path is found in memory index.

          paths_mapping.by_local[paths_search[path_to_compare]] =
            on_disk_id; // map disk id to memory id.
          paths_mapping.by_disk[on_disk_id] =
            paths_search[path_to_compare]; // map memory id to disk id.
          paths_search.erase(
            path_to_compare); // remove already found items so we know later
          // which ones are not on disk.
          // check paths count and if it is different from the new one we create
          // a transaction and add it.

          uint32_t new_path_count = index_to_add.paths_count[paths_mapping.by_disk[on_disk_id]];
          if (disk_io.get_path_count(on_disk_id) !=
            new_path_count) {
            // if it is different we create a transaction to correct it.
            std::string count_overwrite_content = *reinterpret_cast<std::string*>(&new_path_count);
            Transaction count_to_overwrite_path_transaction{
              0,
              5,
              static_cast<uint64_t>((on_disk_id - 1) * 4),
              0,
              1,
              4,
              count_overwrite_content
            };
            transactions.push_back(count_to_overwrite_path_transaction);
          }
        }
        on_disk_count += next_path_end;
      }
      else {
        Log::error("Index: Combine: invalid path content length. Aborting. "
          "The Index could be corrupted.");
      }
      ++on_disk_id;
    }
    else {
      // Abort. A corrupted index would mess things up. If the corruption
      // could not get detected or fixed before here it is most likely broken.
      Log::error("Index: Combine: invalid path content length indicator. "
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
  for (const auto& [key, value] : paths_search) {
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
    Transaction resize_transaction{
      0, 0, 0,
      0, 2, disk_io.get_paths_size() + paths_needed_space
    };
    transactions.push_back(resize_transaction);
    // write it to the now free space at the end of the file.
    Transaction to_add_path_transaction{
      0,
      0,
      static_cast<uint64_t>(disk_io.get_paths_size()),
      0,
      1,
      paths_needed_space,
      paths_add_content
    };
    transactions.push_back(to_add_path_transaction);

    // resize so all paths counts fit.
    Transaction count_resize_transaction{
      0, 5, 0, 0, 2, disk_io.get_paths_count_size() + count_needed_space
    };
    transactions.push_back(count_resize_transaction);
    // write it to the now free space at the end of the file.
    Transaction count_to_add_path_transaction{
      0,
      5,
      (disk_io.get_paths_count_size()),
      0,
      1,
      count_needed_space,
      count_add_content
    };
    transactions.push_back(count_to_add_path_transaction);
  }

  paths_search.clear();

  // copy words_f into memory
  std::vector<WordsFValue> words_f = disk_io.get_words_f();

  // This is needed when we add new words. Each time we add a new word the start
  // of all following chars will change by how much we add. Here we will note
  // down how many now bytes got added at the next char. E.g. new word at 'f'. We
  // add 5 (word is 4 letters + 1 seperator) to the location of 'g' because it's
  // the next occurence. After all words got added we will update the real
  // words_f by combining the current with all the ones that came before. e.g a
  // has 2, b has 5, c has 9. wordsf for b adds 5+2. c = 9+5+2. We have 27
  // because when adding new words we add to + 1 and when its z + 1 its invalid.
  // we will just ignore it
  std::vector<uint64_t> words_F_change(27, 0);
  std::vector<uint64_t> words_F_ID_change(27, 0);

  // local index words needs to have atleast 1 value. Is checked by LocalIndex
  // add_to_disk.
  on_disk_count = 0;
  on_disk_id = 0;
  size_t local_word_count = 0;
  uint8_t local_word_length = index_to_add.words_and_reversed[0].word.length();
  char disk_first_char = 'a';
  char local_first_char =
    index_to_add.words_and_reversed[local_word_count].word[0];

  // check each word on disk. if it is different first letter we will skip using
  // words_f. we will compare chars to chars until they differ. we then know if
  // it is coming first, or we are passed it and need to insert the word. or we
  // are at the same word.

  while (on_disk_count < disk_io.get_words_size()) {
    // If the current word first char is different we use words_f to set the
    // location to the start of that char.
    if (disk_first_char < local_first_char) {
      if (words_f[local_first_char - 'a'].location < disk_io.get_words_size()) {
        Log::write(1, "Index: Merge: Skipping using Words_f Table");
        on_disk_count = words_f[local_first_char - 'a'].location;
        on_disk_id = words_f[local_first_char - 'a'].id;
        disk_first_char = local_first_char;
      }
      else {
        if (words_f[local_first_char - 'a'].location == disk_io.get_words_size()) {
          // Words_f can have entries when if it is the same as words_size it
          // means there are no words for this letter.
          break;
        }
        // This should not happen. Index is corrupted.
        Log::error("Index: Combine: Words_f char value is higher than words "
          "index size. This means the index is corrupted. Reset the "
          "index and report this problem.");
      }
    }

    WORD_SEPARATOR_TYPE word_seperator = disk_io.get_word_separator(on_disk_count);
    if (word_seperator <= 0) {
      // can't be 0 or lower than 0. index corrupt most likely
      Log::error("Index: Combine: Word Seperator is 0 or lower. This can not "
        "be. Index most likely corrupt.");
    }

    char word_first_char = disk_io.get_char_of_word(on_disk_count, 0);
    if (disk_first_char <
      word_first_char) {
      // + WORD_SEPARATOR_SIZE because of the word seperator
      disk_first_char = word_first_char;
    }

    for (int i = 0; i < word_seperator; ++i) {
      // If current chars are the same + word on disk length same as on local
      // length and last char add to existing word
      char disk_c = disk_io.get_char_of_word(on_disk_count, i);
      char local_c = index_to_add.words_and_reversed[local_word_count].word[i];
      if (disk_c == local_c) {
        // If its last char and words are the same length we found it.
        if (i == local_word_length - 1 && word_seperator == local_word_length) {
          // add to existing
          Log::write(1, "Index: Merge: Found existing word");
          add_reversed_to_word(index_to_add, on_disk_count, transactions,
                               additional_new_needed_size,
                               reversed_new_needed_size, on_disk_id,
                               local_word_count, paths_mapping);
          ++local_word_count;
          if (local_word_count ==
            index_to_add.words_and_reversed
                        .size()) {
            // if not more words to compare quit.
            break;
          }
          local_word_length =
            index_to_add.words_and_reversed[local_word_count].word.length();
          on_disk_count +=
            word_seperator +
            WORD_SEPARATOR_SIZE; // word length + the next seperator
          ++on_disk_id;
          local_first_char =
            index_to_add.words_and_reversed[local_word_count].word[0];

          break;
        }
        // If its last char and local word is at the end and not same length
        // means we need to insert before
        if (i == local_word_length - 1) {
          // insert new
          Log::write(1, "Index: Merge: Add new Word");
          add_new_word(index_to_add, on_disk_count, transactions,
                       words_insertions, reversed_insertions,
                       additional_new_needed_size, words_new_needed_size,
                       reversed_new_needed_size, on_disk_id, local_word_count,
                       paths_mapping);
          words_F_change[local_first_char - 'a' + 1] +=
            local_word_length + WORD_SEPARATOR_SIZE;
          ++words_F_ID_change[local_first_char - 'a' + 1];
          ++local_word_count;
          if (local_word_count ==
            index_to_add.words_and_reversed
                        .size()) {
            // if not more words to compare quit.
            break;
          }
          local_word_length =
            index_to_add.words_and_reversed[local_word_count].word.length();
          local_first_char =
            index_to_add.words_and_reversed[local_word_count].word[0];
          break;
        }

        // If it's the last on disk char and at the end and not the same
        // length. means we need to skip this word.
        if (i == word_seperator - 1) {
          // skip
          Log::write(1, "Index: Merge: Skip Word on Disk");
          on_disk_count += word_seperator + WORD_SEPARATOR_SIZE;
          // + then the next seperator
          ++on_disk_id;
          break;
        }
      }

      // If disk char > local char
      if (disk_c > local_c
      ) {
        // insert new
        Log::write(1, "Index: Merge: Add new Word");
        add_new_word(index_to_add, on_disk_count, transactions,
                     words_insertions, reversed_insertions,
                     additional_new_needed_size, words_new_needed_size,
                     reversed_new_needed_size, on_disk_id, local_word_count,
                     paths_mapping);
        words_F_change[local_first_char - 'a' + 1] +=
          local_word_length + WORD_SEPARATOR_SIZE;
        ++words_F_ID_change[local_first_char - 'a' + 1];
        ++local_word_count;
        if (local_word_count ==
          index_to_add.words_and_reversed
                      .size()) {
          // if not more words to compare quit.
          break;
        }
        local_word_length =
          index_to_add.words_and_reversed[local_word_count].word.length();
        local_first_char =
          index_to_add.words_and_reversed[local_word_count].word[0];
        break;
      }

      // If disk char < local char
      if (disk_c < local_c) {
        // skip
        Log::write(1, "Index: Merge: Skip Word on Disk");
        on_disk_count +=
          word_seperator + WORD_SEPARATOR_SIZE; // + then the next seperator
        ++on_disk_id;
        break;
      }
    }
    if (local_word_count == index_to_add.words_and_reversed
                                        .size()) {
      // if not more words to compare quit.
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
    // on_disk_count it doesn't matter because when insertions are
    // processed it will add all the newly added word count to the
    // insertion location.
    Log::write(1, "Index: Merge: Adding a new word at the end");
    add_new_word(index_to_add, on_disk_count, transactions, words_insertions,
                 reversed_insertions, additional_new_needed_size,
                 words_new_needed_size, reversed_new_needed_size, on_disk_id,
                 local_word_count, paths_mapping);
    words_F_change[(index_to_add.words_and_reversed[local_word_count].word[0] -
        'a') +
      1] += local_word_length + WORD_SEPARATOR_SIZE;
    ++words_F_ID_change
      [(index_to_add.words_and_reversed[local_word_count].word[0] - 'a') + 1];
  }

  // calculate the new words f values and create a transaction to update
  // all. This is not an ideal implementation because we copy the whole
  // thing. When we add custom words_f length then we can make a better
  // implementation that is faster and saves memory and space.
  Transaction words_f_new{0, 2, 0, 0, 1, ((8 + WORDS_F_LOCATION_SIZE) * 26)};
  words_f_new.content.resize(((8 + WORDS_F_LOCATION_SIZE) * 26));
  uint64_t all_size = 0;
  uint64_t all_id_change = 0;
  for (int i = 0; i < 26; ++i) {
    all_size += words_F_change[i];
    all_id_change += words_F_ID_change[i];
    words_f[i].location += all_size;
    words_f[i].id += all_id_change;
    std::memcpy(&words_f_new.content[i * (8 + WORDS_F_LOCATION_SIZE)],
                &words_f[i].bytes[0], (8 + WORDS_F_LOCATION_SIZE));
  }
  transactions.push_back(words_f_new);

  words_f_new.content.clear(); // free some memory.

  // Now we have figured out everything we want to do. Now we need to
  // finish the Transaction List and then write it to disk. First add a
  // resize Transaction for words, reversed and additional at the start of
  // the List.
  Log::write(1, "Index: Creating resize transaction for words with size: " +
             std::to_string(disk_io.get_words_size() + words_new_needed_size));
  Log::write(1, "Index: Creating resize transaction for reversed with size: " +
             std::to_string(disk_io.get_reversed_size() + reversed_new_needed_size));
  Log::write(1,
             "Index: Creating resize transaction for additional with size: " +
             std::to_string(disk_io.get_additional_size() + additional_new_needed_size));
  Transaction resize_words{
    0, 1, 0, 0, 2, disk_io.get_words_size() + words_new_needed_size,
    ""
  };
  move_transactions.push_back(resize_words);
  Transaction resize_reversed{
    0, 3, 0, 0, 2, disk_io.get_reversed_size() + reversed_new_needed_size, ""
  };
  move_transactions.push_back(resize_reversed);
  Transaction resize_additional{
    0, 4, 0, 0, 2, disk_io.get_additional_size() + additional_new_needed_size, ""
  };
  move_transactions.push_back(resize_additional);

  // Now we need to convert the Insertion to Transactions.
  // First we need to make Transactions to make place for the insertion.
  // We have already resized so there is enough space for it. We need to
  // create the Transaction so data only gets moved once.
  insertion_to_transactions(transactions, move_transactions, words_insertions,
                            1);
  insertion_to_transactions(transactions, move_transactions,
                            reversed_insertions, 3);

  // Now we need to write the Transaction List to disk.
  // The Transactions are saved in indexpath / transaction /
  // transaction.list Backups are saved in indexpath / transaction /
  // backups / backupid.backup

  std::filesystem::path transaction_path =
    CONFIG_INDEX_PATH / "transaction" / "transaction.list";

  if (write_transaction_file(transaction_path, move_transactions,
                             transactions) == 1) {
    Log::write(
      3, "Index: Merge: error writing transaction file. Check logs above");
    // no return here so we can clean up and later return
  }

  transactions.clear();
  index_to_add.paths.clear();
  index_to_add.paths_count.clear();
  index_to_add.words_and_reversed.clear();

  if (ec)
    return 1;
  return 0;
}
