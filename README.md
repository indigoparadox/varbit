
# varbit

This is a super-simple tool designed to build a database of hashes and
metadata information for a directory of files.

This can be useful for deduplication or for encrypting files for cloud
backup.

It uses the FNV hashing algorithm for cataloging which should be faster than
more cryptographically secure hashing algorithms, but make sure to verify the
results with sha1 or a better algorithm before deleting files.

It's still super-rough. Sorry.

## TODO
- Automatic deduplication.
- Encryption and hashing of contents (probably using external tool).

## Requirements
- sqlite >= 3
- bstrlib (included)

