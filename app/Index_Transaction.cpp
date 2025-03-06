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
    mmap_transactions = mio::make_mmap_sink((index_path / "transaction" / "transaction.list").string(), 0,
                                     mio::map_entire_file, ec);

    while(transaction_current_location + 27 < std::filesystem::file_size(index_path / "transaction" / "transaction.list")) { // if we can read current position + a whole trasactionheader
        TransactionHeader* current_header = reinterpret_cast<TransactionHeader*>(&mmap_transactions[transaction_current_location]); // get the first transaction.
        if (current_header->status == 2) {
            //skip as already complete
            ++transaction_current_id;
            if (current_header->operation_type == 1) {
                transaction_current_location += current_header->content_length;
            }
            transaction_current_location += 27;
            continue;
        } if (current_header->status == 1) {
            // check if need to restore from backup
            if (current_header->backup_id != 0) {
                if (current_header->operation_type == 3) { // If it is a backup creation we just delete the current backup file if it exists and then continue.
                    std::string backup_file_name = std::to_string(current_header->backup_id) + ".backup";
                    std::filesystem::remove(index_path / "transaction" / "backups" / backup_file_name);
                } else {
                    // TODO: restore the backup
                }
            }
        } if (current_header->status == 0 && transaction_current_id == 0) {
            // did not start yet. transaction file not successfully writen. Abort
            log::write(2, "Index: Transaction: transaction list appears to have not finished creating. Deleting only incomplete Transaction List.");
            break;
        } 
        // mark current transaction as in progress. continue executing transaction. mark status as complete.
        
        ++transaction_current_id;
        if (current_header->operation_type == 1) {
            transaction_current_location += current_header->content_length;
        }
        transaction_current_location += 27;
    }


    std::filesystem::remove(index_path / "transaction" / "transaction.list");
    return 0;
  }