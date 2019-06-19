
#ifndef DB_H
#define DB_H

#ifdef USE_SQLITE
#include <sqlite3.h>

#define DB_TYPE sqlite3*
#endif /* USE_SQLITE */

#include "bstrlib.h"
#include "hash.h"

int db_ensure_database( DB_TYPE );
int db_inventory_update_file(
   sqlite3* db, bstring file_path, enum hash_algo hash_type );
void db_list_dupes( DB_TYPE db );

#endif /* DB_H */

