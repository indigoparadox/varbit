
#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include <sqlite3.h>
#include <bstrlib.h>

#include "util.h"
#include "murmurhash.h"

int storage_ensure_database( bstring );
int storage_inventory_update( bstring, bstring );

#endif /* STORAGE_H */

