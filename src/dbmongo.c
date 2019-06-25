
#include "dbmongo.h"

int db_mongo_ensure_database( mongo_connection* db ) {
   return 0;
}

int db_mongo_inventory_update_file(
   mongo_connection* db, bstring file_path, enum hash_algo hash_type
) {
   return 0;
}

int db_mongo_print_dupe( mongo_connection* arg, int cols, char** strs, char** col_names ) {
   return 0;
}

int db_mongo_list_dupes(
   mongo_connection* db, struct db_hash_list* hash_list
) {
   return 0;
}

