
#ifndef DB_H
#define DB_H

#ifdef USE_SQLITE
#include <sqlite3.h>

#define DB_TYPE sqlite3*
#endif /* USE_SQLITE */

#include "bstrlib.h"
#include "hash.h"

struct db_hash_list {
   int len;
   int sz;
   uint8_t** list;
};

int db_ensure_database( DB_TYPE );
int db_inventory_update_file(
   sqlite3* db, bstring file_path, enum hash_algo hash_type );
int db_list_dupes( sqlite3* db, struct db_hash_list* hash_list );

#endif /* DB_H */

