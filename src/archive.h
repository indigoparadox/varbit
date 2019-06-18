
#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>
#include <sys/types.h>

#include "bstrlib.h"
#include "db.h"

#define STORAGE_HASH_FNV 1

#ifdef STORAGE_HASH_MURMUR
#define STORAGE_HASH_BUFFER_SIZE 4
#define STORAGE_HASH_SEED 32
#endif /* STORAGE_HASH_MURMUR */

#ifdef STORAGE_HASH_FNV
#endif /* STORAGE_HASH_FNV */

typedef struct {
   bstring path;
   bstring hardlink_path;
   int mdate;
   ino_t inode;
   off_t size;
   uint32_t hash_contents;
   bstring encrypted_filename;
} storage_file;

int archive_inventory_update_walk( DB_TYPE, bstring );
#ifdef STORAGE_HASH_MURMUR
uint32_t archive_hash_file( bstring );
#endif /* STORAGE_HASH_MURMUR */
#ifdef STORAGE_HASH_FNV
uint64_t archive_hash_file( bstring );
#endif /* STORAGE_HASH_FNV */
void archive_free_storage_file( storage_file* );

#endif /* ARCHIVE_H */

