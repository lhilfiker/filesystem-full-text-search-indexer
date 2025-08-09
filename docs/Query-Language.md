 # Query Language


 - Supported operators are NOT, AND, OR(default if none provided). The operators are case-insensitive, that means however that if you want to search for that specific term it is not possible to just write it in lowercase letters. If you wish to search for it, it is possible by putting it in quotes like this: "NOT", "not", etc...
 - NOT is automatically AND NOT.



 Behaviour:
 - All wildcard results that are found are returned if min char of the word found is reached. It will not stop if a word comes that still is a wildcard match but smaller than min char. Exact match is only added if min char is reached.