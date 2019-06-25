
/* This file is part of varbit.
 * varbit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * varbit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with varbit.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>

#include "bstrlib.h"
#include "util.h"
#include "archive.h"
#include "dbmongo.h"
#include "dbsqlite.h"
#include "hash.h"

enum varbit_action {
   ACTION_NONE = 0,
   ACTION_SCAN,
   ACTION_DEDUP
};

#ifdef USE_THREADPOOL
#include "thpool.h"

#define THREADPOOL_THREADS 4

threadpool g_thpool = NULL;
#endif /* USE_THREADPOOL */
int g_verbose = 0;

static void db_list_dupes(
   void* db, enum db db_type, struct db_hash_list* dupes
) {
   switch( db_type ) {
#ifdef USE_SQLITE
   case VBDB_SQLITE:
      db_sqlite_list_dupes( db, dupes );
      break;
#endif /* USE_SQLITE */
#ifdef USE_MONGO
   case VBDB_MONGO:
      db_mongo_list_dupes( db, dupes );
      break;
#endif /* USE_MONGO */
   }
}

int main( int argc, char** argv ) {
   int arg_iter;
   bstring db_path = NULL;
   bstring arc_path = NULL;
   int retval = 0;
   int storage_retval = 0;
   int sql_retval = 0;
   sqlite3* db;
   enum varbit_action action = ACTION_NONE;
   enum hash_algo hash_type = VBHASH_FNV;
   enum db db_type = VBDB_SQLITE;
   struct db_hash_list dupes = { 0 };

   /* Parse command line arguments. */
   while( ((arg_iter = getopt( argc, argv, "b:sphvd:a:x:" )) != -1) ) {
      switch( arg_iter ) {
         case 'h':

            printf( "Usage: varbit -d <db_path> -a <arc_path> [-v]\n" );
            printf( "This tool will build a database of hashes for all\n" );
            printf( "files in the specified archive directory.\n" );
            printf( "\n" );
            printf( "Actions (Must have at least one):\n" );
            printf( "\n" );
            printf( "-s\t\tScan.\n" );
            printf( "-p\t\tDedup.\n" );
            printf( "\n" );
            printf( "Options:\n" );
            printf( "\n" );
            printf( "-d <db_path>\tThe location of the files database.\n" );
            printf( "-a <arc_path>\tThe directory of the file archive to " \
               "catalog.\n" );
            printf(
               "-x [fnv|mirror|sha256]\tHash to use for comparing files.\n" );
            printf( "-b [sqlite|mongo]\tDatabase to use.\n" );
            printf( "-v\t\tBe verbose.\n" );
            printf( "\n" );
            printf( "The database and archive paths are required.\n" );
         
            /* Exit immediately after showing help. */
            goto cleanup;

         case 'v':
            g_verbose = 1;
            break;

         case 'd':
            db_path = bformat( "%s", optarg );
            break;

         case 'a':
            arc_path = bformat( "%s", optarg );
            break;

         case 'b':
#ifdef USE_SQLITE
            if( 0 == strcmp( "sqlite", optarg ) ) {
               db_type = VBDB_SQLITE;
            } else
#endif /* USE_SQLITE */
#ifdef USE_MONGO
            if( 0 == strcmp( "mongo", optarg ) ) {
               db_type = VBDB_MONGO;
            } else
#endif /* USE_MONGO */
            {
               /* Invalid DB specified. */
            }
            break;

        case 'x':
            if( 0 == strcmp( "fnv", optarg ) ) {
               hash_type = VBHASH_FNV;
#ifdef STORAGE_HASH_MURMUR
            } else if( 0 == strcmp( "murmur", optarg ) ) {
               hash_type = VBHASH_MURMUR;
#endif /* STORAGE_HASH_MURMUR */
            } else if( 0 == strcmp( "sha256", optarg ) ) {
               hash_type = VBHASH_SHA256;
            }
            break;

         case 's':
            action = ACTION_SCAN;
            break;

         case 'p':
            action = ACTION_DEDUP;
            break;

         case ':':
            DBG_ERR( "Option -%c requires an operand.\n", optopt );
            break;
      }
   }

   /* Ensure sanity. */
   CATCH_NULL( db_path, retval, 1, "No database specified. Aborting.\n" );
   CATCH_NULL( arc_path, retval, 1, "No archive specified. Aborting.\n" );

   switch( db_type ) {
#ifdef USE_SQLITE
   case VBDB_SQLITE:
      sql_retval = sqlite3_open( bdata( db_path ), &db );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to open database: %s\n", bdata( db_path )
      );

      /* TODO: Perform operations other than update. */

      storage_retval = db_sqlite_ensure_database( db );
      CATCH_NONZERO(
         storage_retval, retval, 1, "Error ensuring database tables. Aborting.\n"
      );

      break;
#endif /* USE_SQLITE */
#ifdef USE_MONGO
   case VBDB_MONGO:
      /* TODO */
      break;
#endif /* USE_MONGO */
   }

#ifdef USE_THREADPOOL
   g_thpool = thpool_init( THREADPOOL_THREADS );
#endif /* USE_THREADPOOL */

   switch( action ) {
      case ACTION_SCAN:
         storage_retval = archive_inventory_update_walk(
            db, db_type, arc_path, hash_type );
         CATCH_NONZERO(
            storage_retval, retval, 1, "Error updating inventory. Aborting.\n"
         );
         break;

      case ACTION_DEDUP:
         db_list_dupes( db, db_type, &dupes );
         break;

      case ACTION_NONE:
      default:
         break;
   }

   /* Wait for worker threads to finish up. */
#ifdef USE_THREADPOOL
   thpool_wait( g_thpool );
#endif /* USE_THREADPOOL */

cleanup:

   if( NULL != db_path ) {
      bdestroy( db_path );
   }

   if( NULL != arc_path ) {
      bdestroy( arc_path );
   }

   if( NULL != db ) {
      sqlite3_close( db );
   }

   return retval;
}

