# Configuration System
The filesystem full-text search indexer uses a simple key-value configuration system that supports both file-based configuration and command-line overrides.

## Configuration File Format

The configuration file uses a simple key=value format with one setting per line:
`key=value`

### Format Rules

- No spaces around equals sign: Use key=value, not key = value
- One setting per line: Each configuration option must be on its own line
- Invalid lines: If a line is invalid, e.g misspelled config key or empty lines or comments are ignored.
- Case sensitive: All keys and values are case-sensitive

## Configuration Loading Priority
The configuration system loads settings in the following order (later sources override earlier ones):

Default values - Built-in defaults from the application
Configuration file - Values from the specified config file
Command-line arguments - CLI options override both defaults and file settings

## Quirks and Important Notes
File Handling

Missing config file is not an error - The application will continue with defaults and CLI options if the config file doesn't exist
Partial configs are valid - You only need to specify the settings you want to change from defaults
Invalid lines are silently ignored

### Boolean Values
Boolean settings accept string values that are converted:

True values: "true"
False values: "false", anything else (including empty strings)

### Numeric Values

All numeric settings are parsed as integers using std::stoi()
Memory values are in bytes - For config_local_index_memory, remember that 1MB = 1,000,000 bytes

### Path Values

Use full absolute paths - Relative paths may not work as expected
No path validation - The config system doesn't verify that paths exist or are accessible
Required paths - Both index_path and config_path_to_scan must be set (either in config file or via CLI)

## All Config values accepted:

| Key      | Allowed Values      | Default Value      | Part      | Required      |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| index_path | full path | none | Index | Yes |
| lock_acquisition_timeout | number in seconds | 30 | Index | No |
| config_scan_dot_paths | true/false | false | Indexer | No |
| config_path_to_scan | full path | none | Indexer | Yes |
| config_updated_files_only | true/false | true | Indexer | No |
| config_threads_to_use | number of threads to use | 1 | Indexer | No |
| config_local_index_memory | number in bytes | 50000 | Indexer | No |
| config_min_word_length | number of letters it needs to be | 4 | Indexer & Search | No |
| config_max_word_length | number of letters it can max to be | 230 | Indexer & Search | No |
| config_min_log_level | 1(debug), 2(info), 3(warn), 4(critical) | 3 | Logger | No |
| config_exact_match | true/false | false | Search | No |
| config_min_char_for_match | integer | 4 | Search | No |
