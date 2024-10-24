# index files structure
Multiple files are written to disk which the index is saved on. By default they will be written to .local/share/filesystem-full-text-search-indexer. This can be changed in a future release.

The index is divided into different categories which are saved to different files in different formats.
| Filename | Function | Format / Type |
| -------- | -------- | ------------- |
| paths.index | This saves the whole path of all files indexed. | All paths are divided into chars which are saved to the file. To seperate a newline character is added in between paths. The first item starts at 1 because additional and reversed need 0 to signal empty value. All other indexes are starting from 0. |
| paths_count.index | This saves the word count of each path from paths.index | Each word count is saved in 4 bytes. There is no seperator. E.g byte 4-7 belong to path id 2. |
| words.index | This saves each word once from all documents. | It is saved alphabeticly sorted. Each word is split into chars. Each char is converted into a 5 bit value (a is 0, b is 1...). To signal a new word a 5 bit value of '29' is added. At the end of the file a 5 bit value of '30' is added. It will be saved as 8 bits ( 1Byte) to the file. E.g 1 Byte in the file holds the complete 5 bit char of the first word. The same byte also holds the 3 bits of the next char. The second byte hold the last 2 bits of the second char and the next full 5 bit char and then only 1 bit of the next 5 bit char. If the file is done, the 5 bit value of '30' signals that and the rest of the byte if not full is set to 0. |
| words_f.index | This saves the id of the first occurence of a new letter in the alphabet from words.index | Holds 26 values for each letter in the alphabet to show where that letter first occurs. Because words.index is sorted it allows to quickly search only a specific part of the words.index based on the first letter of the word. It is saved as 8 bytes per value. |
| reversed.index | This saves the path_ids that are linked to a specific word. It's space is by default limited and it will refrence additional id if it needs space for more path_ids. |
| additional | This provides space for more path_ids for additional. It can also refrence other addition ids if the current additonal ids also runs out of space. |


**Reversed Index File Structure**

Each word has an associated block with a matching index ID. Finding the reversed block for an index ID is very efficient since each block has a fixed length of 10 bytes (configurable in future versions). The block's starting position can be calculated as: `(index_id × 10) - 10`.

**Block Structure**

| Bytes 1-2 | Bytes 3-4 | Bytes 5-6 | Bytes 7-8 | Bytes 9-10 |
| path_id   | path_id   | path_id   | path_id   | additional_id |

Each block can store up to 4 path IDs. Due to using 2-byte values, the current design limits the system to 65,536 different paths. This limit can be modified during initial setup or may be automatically adjusted in future versions.

**Additional Blocks**
The additional_id field links to an additional block, which follows a similar structure but is 50 bytes long. Each additional block can store:
- 24 path IDs
- 1 additional_id (for linking to the next block if needed)

Examples:

1. **Word with 3 paths:**
   - Creates a reversed block
   - First 6 bytes contain 3 path IDs
   - Fourth path_id slot (bytes 7-8) is set to 0
   - additional_id (bytes 9-10) is set to 0

2. **Word with 20 paths:**
   - Creates a reversed block
   - All 4 path_id slots are populated
   - Sets an additional_id to link to a new 50-byte block
   - In the additional block:
     - First 16 path_id slots are populated
     - Remaining 8 slots are set to 0 (available for future use)

3. **Word with more paths:**
   - System creates new additional blocks as needed
   - Each block links to the next via the additional_id field
   - Forms a chain of blocks to accommodate any number of paths
