#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

void Index::write_to_transaction(std::vector<Transaction> &transactions,
                                 mio::mmap_sink &mmap_transactions,
                                 size_t &transaction_file_location) {
  std::error_code ec;
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
  if (ec)
    Log::error("Index: Write to Transaction Failed. Exiting");
}

int Index::write_transaction_file(const std::filesystem::path &transaction_path,
                                  std::vector<Transaction> &move_transactions,
                                  std::vector<Transaction> &transactions) {
  // writes the Transactionf file and does some extra checks
  std::error_code ec;

  // we remove all backups files because we will write now a new transaction
  // file.
  std::filesystem::remove_all(CONFIG_INDEX_PATH / "transaction" / "backups");
  if (!std::filesystem::is_directory(CONFIG_INDEX_PATH / "transaction" /
                                     "backups")) {
    Log::write(1,
               "Index: intitialize: creating transactions backups directory.");
    std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction" /
                                        "backups");
  }

  size_t recalculated_size = 0;
  for (const auto &tx : transactions) {
    recalculated_size += 27; // Header size
    if (tx.header.operation_type != 2 && tx.header.operation_type != 3) {
      recalculated_size += tx.header.content_length;
    }
  }
  // only move and backup transactions
  for (const auto &tx : move_transactions) {
    recalculated_size += 27; // Header size
    if (tx.header.operation_type != 3) {
      recalculated_size += 8; // Move operations need 8 bytes
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
  // First write the resize and move operations, then the writes so we can do
  // writes without syncing to disk everytime speeding it up
  write_to_transaction(move_transactions, mmap_transactions,
                       transaction_file_location);
  write_to_transaction(transactions, mmap_transactions,
                       transaction_file_location);

  // Transaction List written. unmap and free memory.
  mmap_transactions.sync(ec);
  // mark first as started to signal that writing was successful.
  mmap_transactions[0] = 1; // first item status to 1
  mmap_transactions.sync(ec);
  mmap_transactions.unmap();
  if (ec)
    return 1;
  return 0;
}

int Index::execute_transactions() {
  Log::write(2, "Index: Transaction: Beginning execution of transaction list");
  std::error_code ec;
  size_t transaction_current_id = 0;
  size_t transaction_current_write_sync_batch = 0;
  size_t transaction_current_location = 0;
  mio::mmap_sink mmap_transactions;
  mmap_transactions = mio::make_mmap_sink(
      (CONFIG_INDEX_PATH / "transaction" / "transaction.list").string(), 0,
      mio::map_entire_file, ec);
  /*Log::write(1, "Index: Transaction: Detected " +
                    std::to_string(
                        std::filesystem::file_size(CONFIG_INDEX_PATH /
     "transaction" / "transaction.list") / 27) + " potential transactions");
  */
  if (ec) {
    Log::write(3, "Transaction Execution failed, could not map file.");
    return 1;
  }

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
    Log::write(
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
      Log::write(
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
        Log::write(1, "Index: Transaction: Processing backup ID " +
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
            Log::error("Transaction: backup file corrupt. Can not restore from "
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
      Log::write(
          2, "Index: Transaction: transaction list appears to have not "
             "finished creating. Deleting only incomplete Transaction List.");
      break;
    }
    // continue executing transaction.
    Log::write(1, "Index: Transaction: Executing transaction #" +
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
      Log::write(1, "Index: Transaction: Move operation completed for index " +
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
      Log::write(1, "Index: Transaction: Write operation completed for index " +
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
      Log::write(
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
      Log::write(
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
        Log::error(
            "Error when syncing indexes to disk. Exiting Program to save "
            "data. Please restart to see if the issue continues.");
      } else {
        Log::write(
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
    Log::write(1, "Index: Transaction: Completed transaction #" +
                      std::to_string(transaction_current_id - 1));
  }
  Log::write(2, "Index: Transaction: Successfully completed " +
                    std::to_string(transaction_current_id) + " transactions");

  Log::write(2,
             "Index: Transaction: Cleaning up transaction files and backups");

  std::filesystem::remove(CONFIG_INDEX_PATH / "transaction" /
                          "transaction.list");
  // removing all backups because they are not needed anymore.
  std::filesystem::remove_all(CONFIG_INDEX_PATH / "transaction" / "backups");
  std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction" /
                                      "backups");
  Log::write(2, "Index: Transaction: Execution finished.");
  return 0;
}