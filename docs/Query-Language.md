# **Query Language Documentation**

## **Basic Syntax**
- **Required Format**: All queries must be wrapped in parentheses: `(search terms)` (added by default if using Search::search()) 
- **Word Length**: 4-253 characters (configurable minimum)
- **Case**: Case-insensitive search (uses Helper::convert-char, converts according to it. Future sensitivity may be configured there)

## **Operators**
| Operator | Function | Example |
|----------|----------|---------|
| **OR** | Find ANY term (default) | `(apple banana)` = either apple OR banana |
| **AND** | Find ALL terms | `(apple AND banana)` = both required |
| **NOT** | Exclude term (acts as AND NOT) | `(apple NOT banana)` = apple without banana |

- Operators are case-insensitive (`AND` = `and` = `And`)
- Evaluated left-to-right
- Use nested parentheses for complex queries: `((apple OR banana) AND fresh)`

## **Search Modes**

### **Wildcard (Default)**
- Matches any word containing the search term
- Example: `(app)` matches "apple", "application", "app"
- For words to be found, they need to be atleast 4 letters(by default) long. This can be configured globaly via the config_min_char_for_match option.

### **Exact Match**
- Use double quotes: `"apple"` matches only "apple"
- Required for searching operator keywords: `"AND"`, `"OR"`, `"NOT"`
- Can not search phrases.

## **Examples**
```
(apple)                           # Find "apple"
(apple banana)                    # Find apple OR banana
(apple AND banana)                # Find both
(apple NOT banana)                # Apple without banana
("exact match" wildcard)          # Mix exact and wildcard
((fruit OR vegetable) AND fresh)  # Nested query
```

## **Important Notes**
- Special characters are ignored/converted
- Results include `path_id` and `count` (match frequency)
- Higher count = more relevant result