| Key      | Allowed Values      | Default Value      | Part      | Required      |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| index_path | full path | none | Index | Yes |
| lock_aquisition_timeout | number in seconds | 30 | Index | No |
| config_scan_dot_paths | true/false | false | Indexer | No |
| config_path_to_scan | full path | none | Indexer | Yes |
| threads_to_use | number of threads to use | 1 | Indexer | No |
| config_local_index_memory | number in bytes | 50000 | Indexer | No |
| config_min_log_level | 1(debug), 2(info), 3(warn), 4(critical) | 3 | Logger | No |
| config_exact_match | true/false | false | Search | No |
| config_min_char_for_match | integer | 4 | Search | No |
