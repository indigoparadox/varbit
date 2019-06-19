
#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>
#include <sys/types.h>

#include "bstrlib.h"
#include "db.h"
#include "hash.h"

typedef struct {
   bstring path;
   bstring hardlink_path;
   int mdate;
   ino_t inode;
   off_t size;
   uint8_t hash_contents[HASH_MAX_LEN];
   enum hash_algo hash_type;
   bstring encrypted_filename;
} storage_file;

int archive_inventory_update_walk(
   DB_TYPE db, bstring archive_path, enum hash_algo hash_type );
void archive_hash_file(
   bstring file_path, enum hash_algo hash_type, uint8_t hash[HASH_MAX_LEN] );
void archive_free_storage_file( storage_file* );

#endif /* ARCHIVE_H */

