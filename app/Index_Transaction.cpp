#include "functions.h"
#include "lib/mio.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

int Index::execute_transactions() {
  log::write(2, "Index: Transaction: Beginning execution of transaction list");
  std::error_code ec;
  size_t transaction_current_id = 0;
  size_t transaction_current_write_sync_batch = 0;
  size_t transaction_current_location = 0;
  mio::mmap_sink mmap_transactions;
  mmap_transactions = mio::make_mmap_sink(
      (CONFIG_INDEX_PATH / "transaction" / "transaction.list").string(), 0,
      mio::map_entire_file, ec);
  /*log::write(1, "Index: Transaction: Detected " +
                    std::to_string(
                        std::filesystem::file_size(CONFIG_INDEX_PATH /
     "transaction" / "transaction.list") / 27) + " potential transactions");
  */
  while (transaction_current_location + 27 <
         std::filesystem::file_size(
             CONFIG_INDEX_PATH / "transaction" /
             "transaction.list")) { // if we can read current position + a whole
                                    // trasactionheader
    TransactionHeader *current_header = reinterpret_cast<TransactionHeader *>(
        &mmap_transactions
            [transaction_current_location]); // get the first
                                             // transaction.
                                             // In execute_transactions, right
                                             // after loading the header
    log::write(
        1, "Transaction header dump - status: " +
               std::to_string(current_header->status) +
               ", index_type: " + std::to_string(current_header->index_type) +
               ", location: " + std::to_string(current_header->location) +
               ", backup_id: " + std::to_string(current_header->backup_id) +
               ", operation_type: " +
               std::to_string(current_header->operation_type) +
               ", content_length: " +
               std::to_string(current_header->content_length));
    if (current_header->status == 2) {
      log::write(
          1, "Index: Transaction: Skipping already completed transaction #" +
                 std::to_string(transaction_current_id));
      // skip as already complete
      ++transaction_current_id;
      if (current_header->operation_type == 1) {
        transaction_current_location += current_header->content_length;
      }
      if (current_header->operation_type == 0) {
        transaction_current_location += 8;
      }
      transaction_current_location += 27;
      continue;
    }
    if (current_header->status == 1) {
      // check if need to restore from backup
      if (current_header->backup_id != 0) {
        log::write(1, "Index: Transaction: Processing backup ID " +
                          std::to_string(current_header->backup_id) +
                          " for transaction #" +
                          std::to_string(transaction_current_id));
        std::string backup_file_name =
            std::to_string(current_header->backup_id) + ".backup";
        if (current_header->operation_type ==
            3) { // If it is a backup creation we just delete the current backup
                 // file if it exists and then continue.
          std::filesystem::remove(CONFIG_INDEX_PATH / "transaction" /
                                  "backups" / backup_file_name);
        } else {
          if (current_header->content_length !=
              std::filesystem::file_size(CONFIG_INDEX_PATH / "transaction" /
                                         "backups" / backup_file_name)) {
            log::error("Transaction: backup file corrupt. Can not restore from "
                       "backup. Index potentially corrupt. Exiting. This "
                       "should never happen.");
          }
          mio::mmap_sink mmap_backup;
          mmap_backup = mio::make_mmap_sink(
              (CONFIG_INDEX_PATH / "transaction" / "backups" / backup_file_name)
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
    // continue executing transaction.
    log::write(1, "Index: Transaction: Executing transaction #" +
                      std::to_string(transaction_current_id) + " type=" +
                      std::to_string(current_header->operation_type) +
                      " index=" + std::to_string(current_header->index_type));

    if (current_header->operation_type == 0) { // MOVE
      current_header->status = 1;
      mmap_transactions.sync(ec);
      uint64_t shift_amount = 0;
      memcpy(&shift_amount,
             &mmap_transactions[transaction_current_location + 27], 8);
      std::memmove(
          current_header->index_type == 0
              ? &mmap_paths[current_header->location + shift_amount]
              : (current_header->index_type == 1
                     ? &mmap_words[current_header->location + shift_amount]
                     : (current_header->index_type == 2
                            ? &mmap_words_f[current_header->location +
                                            shift_amount]
                            : (current_header->index_type == 3
                                   ? &mmap_reversed[current_header->location +
                                                    shift_amount]
                                   : (current_header->index_type == 4
                                          ? &mmap_additional[current_header
                                                                 ->location +
                                                             shift_amount]
                                          : &mmap_paths_count[current_header
                                                                  ->location +
                                                              shift_amount])))),
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
          current_header->content_length);
      log::write(1, "Index: Transaction: Move operation completed for index " +
                        std::to_string(current_header->index_type) +
                        " at location " +
                        std::to_string(current_header->location));
    } else if (current_header->operation_type == 1) { // WRITE
      if (transaction_current_write_sync_batch == 0 ||
          transaction_current_write_sync_batch > 5000) {
        current_header->status = 1;
        mmap_transactions.sync(ec);
        transaction_current_write_sync_batch = 1;
      }
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
      log::write(1, "Index: Transaction: Write operation completed for index " +
                        std::to_string(current_header->index_type) +
                        " at location " +
                        std::to_string(current_header->location));
      ++transaction_current_write_sync_batch;
    } else if (current_header->operation_type == 2) { // RESIZE
      current_header->status = 1;
      mmap_transactions.sync(ec);
      unmap();
      resize(current_header->index_type == 0
                 ? CONFIG_INDEX_PATH / "paths.index"
                 : (current_header->index_type == 1
                        ? CONFIG_INDEX_PATH / "words.index"
                        : (current_header->index_type == 2
                               ? CONFIG_INDEX_PATH / "words_f.index"
                               : (current_header->index_type == 3
                                      ? CONFIG_INDEX_PATH / "reversed.index"
                                      : (current_header->index_type == 4
                                             ? CONFIG_INDEX_PATH /
                                                   "additional.index"
                                             : CONFIG_INDEX_PATH /
                                                   "paths_count.index")))),
             current_header->content_length);
      switch (current_header->index_type) {
      case 0:
        paths_size = current_header->content_length;
        break;
      case 1:
        words_size = current_header->content_length;
        break;
      case 2:
        words_f_size = current_header->content_length;
        break;
      case 3:
        reversed_size = current_header->content_length;
        break;
      case 4:
        additional_size = current_header->content_length;
        break;
      default:
        paths_count_size = current_header->content_length;
        break;
      }
      map();
      log::write(
          1, "Index: Transaction: Resize operation completed for index " +
                 std::to_string(current_header->index_type) + " and size: " +
                 std::to_string(current_header->content_length));
    } else if (current_header->operation_type == 3) { // CREATE A BACKUP
      current_header->status = 1;
      mmap_transactions.sync(ec);
      std::string backup_file_name =
          std::to_string(current_header->backup_id) + ".backup";
      std::ofstream{CONFIG_INDEX_PATH / "transaction" / "backups" /
                    backup_file_name};
      resize(CONFIG_INDEX_PATH / "transaction" / "backups" / backup_file_name,
             current_header->content_length);
      mio::mmap_sink mmap_backup;
      mmap_backup = mio::make_mmap_sink(
          (CONFIG_INDEX_PATH / "transaction" / "backups" / backup_file_name)
              .string(),
          0, mio::map_entire_file, ec);
      std::memcpy(
          &mmap_backup[0],
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
          current_header->content_length);
      mmap_backup.sync(ec);
      mmap_backup.unmap();
      log::write(
          1, "Index: Transaction: Backup operation completed for index " +
                 std::to_string(current_header->index_type) + " at location " +
                 std::to_string(current_header->location));
    }

    // snyc before we mark as done.
    if (current_header->operation_type != 1 ||
        transaction_current_write_sync_batch > 5000 ||
        transaction_current_write_sync_batch ==
            0) { // sync only if it was not a move operation or 5000 move
                 // operations happend
      if (sync_all() == 1) {
        log::error(
            "Error when syncing indexes to disk. Exiting Program to save "
            "data. Please restart to see if the issue continues.");
      } else {
        log::write(
            1, "Index: Transaction: Successfully synced index changes to disk");
      }
      transaction_current_write_sync_batch = 0;
    }

    // mark current transaction as in completed and sync to disk.
    if (current_header->operation_type != 1) { // only sync if it is not a move
      current_header->status = 2;
      mmap_transactions.sync(ec);
    }
    ++transaction_current_id;
    if (current_header->operation_type == 1) {
      transaction_current_location += current_header->content_length;
    }
    if (current_header->operation_type == 0) {
      transaction_current_location += 8;
    }
    transaction_current_location += 27;
    log::write(1, "Index: Transaction: Completed transaction #" +
                      std::to_string(transaction_current_id - 1));
  }
  log::write(2, "Index: Transaction: Successfully completed " +
                    std::to_string(transaction_current_id) + " transactions");

  log::write(2,
             "Index: Transaction: Cleaning up transaction files and backups");

  std::filesystem::remove(CONFIG_INDEX_PATH / "transaction" /
                          "transaction.list");
  // removing all backups because they are not needed anymore.
  std::filesystem::remove_all(CONFIG_INDEX_PATH / "transaction" / "backups");
  std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction" /
                                      "backups");
  log::write(2, "Index: Transaction: Execution finished.");
  return 0;
}