
#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <bstrlib.h>

#include "util.h"
#include "murmurhash.h"

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
int storage_sql_storage_file( sqlite3_stmt*, storage_file* );
void storage_free_storage_file( storage_file* );

#endif /* STORAGE_H */

