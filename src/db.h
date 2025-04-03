
#ifndef DB_H
#define DB_H

#include "bstrlib.h"
#include "hash.h"

enum db {
#ifdef USE_SQLITE
   VBDB_SQLITE = 2
#endif /* USE_MONGO */
};

struct db_hash_list {
   int len;
   int sz;
   bstring* list;
};

#endif /* DB_H */

