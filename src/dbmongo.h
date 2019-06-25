
#ifndef DBMONGO_H
#define DBMONGO_H

#include <mongo.h>

#include "bstrlib.h"
#include "hash.h"
#include "db.h"

int db_mongo_ensure_database( mongo_connection* );
int db_mongo_inventory_update_file(
   mongo_connection*, bstring, enum hash_algo );
int db_mongo_list_dupes( mongo_connection* db, struct db_hash_list* hash_list );

#endif /* DBMONGO_H */

