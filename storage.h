
#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <bstrlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "util.h"

/* #define STORAGE_HASH_MURMUR 1 */
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

int storage_ensure_database( bstring );
int storage_inventory_update_file( sqlite3*, bstring );
int storage_inventory_update_walk( bstring, bstring );
#ifdef STORAGE_HASH_MURMUR
uint32_t storage_hash_file( bstring );
#endif /* STORAGE_HASH_MURMUR */
#ifdef STORAGE_HASH_FNV
uint64_t storage_hash_file( bstring );
#endif /* STORAGE_HASH_FNV */
int storage_sql_storage_file( sqlite3_stmt*, storage_file* );
void storage_free_storage_file( storage_file* );

#endif /* STORAGE_H */

