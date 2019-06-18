
#ifndef DB_H
#define DB_H

#ifdef USE_SQLITE
#include <sqlite3.h>

#define DB_TYPE sqlite3*
#endif /* USE_SQLITE */

#include "bstrlib.h"

int db_ensure_database( DB_TYPE );
int db_inventory_update_file( DB_TYPE db, bstring file_path );
void db_list_dupes( DB_TYPE db );

#endif /* DB_H */

