#include "functions.h"
#include "lib/mio.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

int Index::execute_transactions() {
  std::error_code ec;
  size_t transaction_current_id = 0;
  size_t transaction_current_location = 0;
  mio::mmap_sink mmap_transactions;
  mmap_transactions = mio::make_mmap_sink(
      (index_path / "transaction" / "transaction.list").string(), 0,
      mio::map_entire_file, ec);

  while (transaction_current_location + 27 <
         std::filesystem::file_size(
             index_path / "transaction" /
             "transaction.list")) { // if we can read current position + a whole
                                    // trasactionheader
    TransactionHeader *current_header = reinterpret_cast<TransactionHeader *>(
        &mmap_transactions[transaction_current_location]); // get the first
                                                           // transaction.
    if (current_header->status == 2) {
      // skip as already complete
      ++transaction_current_id;
      if (current_header->operation_type == 1) {
        transaction_current_location += current_header->content_length;
      }
      transaction_current_location += 27;
      continue;
    }
    if (current_header->status == 1) {
      // check if need to restore from backup
      if (current_header->backup_id != 0) {
        std::string backup_file_name =
            std::to_string(current_header->backup_id) + ".backup";
        if (current_header->operation_type ==
            3) { // If it is a backup creation we just delete the current backup
                 // file if it exists and then continue.
          std::filesystem::remove(index_path / "transaction" / "backups" /
                                  backup_file_name);
        } else {
          if (current_header->content_length !=
              std::filesystem::file_size(index_path / "transaction" /
                                         "backups" / backup_file_name)) {
            log::error("Transaction: backup file corrupt. Can not restore from "
                       "backup. Index potentially corrupt. Exiting. This "
                       "should never happen.");
          }
          mio::mmap_sink mmap_backup;
          mmap_backup = mio::make_mmap_sink(
              (index_path / "transaction" / "backups" / backup_file_name)
                  .string(),
              0, mio::map_entire_file, ec);
          // restore from backup. overwrite then continue.
          if (current_header->index_type == 1) {
            // words index
            std::memcpy(&mmap_words[current_header->location], &mmap_backup[0],
                        current_header->content_length);
          }
          if (current_header->index_type == 3) {
            // reversed index.
            std::memcpy(&mmap_reversed[current_header->location],
                        &mmap_backup[0], current_header->content_length);
          }
        }
      }
    }
    if (current_header->status == 0 && transaction_current_id == 0) {
      // did not start yet. transaction file not successfully writen. Abort
      log::write(
          2, "Index: Transaction: transaction list appears to have not "
             "finished creating. Deleting only incomplete Transaction List.");
      break;
    }
    // mark current transaction as in progress and sync to disk.
    current_header->status = 1;
    mmap_transactions.sync(ec);
    // continue executing transaction.

    if (current_header->operation_type == 0) { // MOVE
      std::memmove(
          current_header->index_type == 0
              ? &mmap_paths[current_header->location]
              : (current_header->index_type == 1
                     ? &mmap_words[current_header->location]
                     : (current_header->index_type == 2
                            ? &mmap_words_f[current_header->location]
                            : (current_header->index_type == 3
                                   ? &mmap_reversed[current_header->location]
                                   : (current_header->index_type == 4
                                          ? &mmap_additional[current_header
                                                                 ->location]
                                          : &mmap_paths_count
                                                [current_header->location])))),
          &mmap_transactions[transaction_current_location + 27],
          current_header->content_length);
    } else if (current_header->operation_type == 1) { // WRITE
        std::memcpy(
            current_header->index_type == 0
                ? &mmap_paths[current_header->location]
                : (current_header->index_type == 1
                       ? &mmap_words[current_header->location]
                       : (current_header->index_type == 2
                              ? &mmap_words_f[current_header->location]
                              : (current_header->index_type == 3
                                     ? &mmap_reversed[current_header->location]
                                     : (current_header->index_type == 4
                                            ? &mmap_additional[current_header
                                                                   ->location]
                                            : &mmap_paths_count
                                                  [current_header->location])))),
            &mmap_transactions[transaction_current_location + 27],
            current_header->content_length);
    } else if (current_header->operation_type == 2) { // RESIZE

    } else if (current_header->operation_type == 3) { // CREATE A BACKUP
    }

    // snyc before we mark as done.
    mmap_transactions.sync(ec);

    // mark current transaction as in completed and sync to disk.
    current_header->status = 2;
    mmap_transactions.sync(ec);

    ++transaction_current_id;
    if (current_header->operation_type == 1) {
      transaction_current_location += current_header->content_length;
    }
    transaction_current_location += 27;
  }

  std::filesystem::remove(index_path / "transaction" / "transaction.list");
  // removing all backups because they are not needed anymore.
  std::filesystem::remove_all(index_path / "transaction" / "backups");
  std::filesystem::create_directories(index_path / "transaction" / "backups");

  return 0;
}