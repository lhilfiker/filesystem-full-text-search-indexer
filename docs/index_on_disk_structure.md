# index files structure
Multiple files are written to disk which the index is saved on. By default they will be written to .local/share/filesystem-full-text-search-indexer. This can be changed in a future release.

The index is divided into different categories which are saved to different files in different formats.
| Filename | Function | Format / Type |
| -------- | -------- | ------------- |
| paths.index | This saves the whole path of all files indexed. | All paths are divided into chars which are saved to the file. To seperate a newline character is added in between paths. |
| paths_count.index | This saves the word count of each path from paths.index | Each word count is saved in 4 bytes. There is no seperator. E.g byte 4-7 belong to path id 2. |
| words.index | This saves each word once from all documents. | It is saved alphabeticly sorted. Each word is split into chars. Each char is converted into a 5 bit value (a is 0, b is 1...). To signal a new word a 5 bit value of '29' is added. At the end of the file a 5 bit value of '30' is added. It will be saved as 8 bits ( 1Byte) to the file. E.g 1 Byte in the file holds the complete 5 bit char of the first word. The same byte also holds the 3 bits of the next char. The second byte hold the last 2 bits of the second char and the next full 5 bit char and then only 1 bit of the next 5 bit char. If the file is done, the 5 bit value of '30' signals that and the rest of the byte if not full is set to 0. |
| words_f.index | This saves the id of the first occurence of a new letter in the alphabet from words.index | Holds 26 values for each letter in the alphabet to show where that letter first occurs. Because words.index is sorted it allows to quickly search only a specific part of the words.index based on the first letter of the word. It is saved as 8 bytes per value. |
| reversed.index | This saves the path_ids that are linked to a specific word. It's space is by default limited and it will refrence additional id if it needs space for more path_ids. |
| additional | This provides space for more path_ids for additional. It can also refrence other addition ids if the current additonal ids also runs out of space. |

