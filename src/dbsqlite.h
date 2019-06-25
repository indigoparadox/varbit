
#ifndef DBSQLITE_H
#define DBSQLITE_H

#include <sqlite3.h>

#include "bstrlib.h"
#include "hash.h"
#include "db.h"

int db_sqlite_ensure_database( sqlite3* );
int db_sqlite_inventory_update_file(
   sqlite3* db, bstring file_path, enum hash_algo hash_type );
int db_sqlite_list_dupes( sqlite3* db, struct db_hash_list* hash_list );

#endif /* DBSQLITE_H */

