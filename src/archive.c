
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

#include "archive.h"

#include <dirent.h>
#include <sys/types.h>
#include <stdlib.h>

#include "util.h"
#include "hash.h"

#ifdef USE_THREADPOOL
#include "thpool.h"

extern threadpool g_thpool;
#endif /* USE_THREADPOOL */

struct archive_path_db {
   DB_TYPE db;
   bstring subdir_path;
   enum hash_algo hash_type;
};

void db_inventory_update_file_thd( struct archive_path_db* thargs ) {
   db_inventory_update_file(
      thargs->db, thargs->subdir_path, thargs->hash_type );
   bdestroy( thargs->subdir_path );
   free( thargs );
}

int archive_inventory_update_walk(
   DB_TYPE db, bstring archive_path, enum hash_algo hash_type
) {
   int retval = 0;
   int bstr_retval = 0;
   DIR* dir = NULL;
   struct dirent* entry = NULL;
   bstring subdir_path = bfromcstr( "" );
   char* archive_path_c = NULL;
#ifdef USE_THREADPOOL
   struct archive_path_db* thargs = NULL;
#endif /* USE_THREADPOOL */

   archive_path_c = bdata( archive_path );
   dir = opendir( archive_path_c );
   CATCH_NULL(
      dir, retval, 1, "Unable to open directory: %s\n", bdata( archive_path )
   );

   while( (entry = readdir( dir )) != NULL ) {
      if( '.' == entry->d_name[0] ) {
         /* Skip hidden directories. */
         /* TODO: Don't skip all hidden files. */
         continue;
      }

      bstr_retval = bassignformat(
         subdir_path, "%s/%s", bdata( archive_path ), entry->d_name
      );
      CATCH_BSTRERR( bstr_retval, retval, 1 );

      if( DT_DIR == entry->d_type ) {
         /* Walk subdirectories. */
         archive_inventory_update_walk( db, subdir_path, hash_type );
         continue;
      } else if( DT_REG == entry->d_type ) {
         /* Record file attributes. */
#ifdef USE_THREADPOOL
         thargs = calloc( 1, sizeof( thargs ) );
         thargs->subdir_path = bstrcpy( subdir_path );
         thargs->db = db;
         thargs->hash_type = hash_type;
         thpool_add_work(
            g_thpool, (void*)db_inventory_update_file_thd, (void*)thargs );
#else
         db_inventory_update_file( db, subdir_path, hash_type );
#endif /* USE_THREADPOOL */
      } else if( DT_LNK == entry->d_type ) {
         /* TODO: Record link existence. */
      }
   }

cleanup:

   bdestroy( subdir_path );

   if( NULL != dir ) {
      closedir( dir );
   }

   return retval;
}

void archive_hash_file(
   bstring file_path, enum hash_algo hash_type, uint8_t hash[HASH_MAX_LEN]
) {
   uint32_t hash_temp_32 = 0;
   uint64_t hash_temp_64 = 0;

   switch( hash_type ) {
#ifdef STORAGE_HASH_FNV
   case VBHASH_FNV:
      hash_temp_64 = hash_file_fnv( file_path );
      memcpy( hash, &hash_temp_64, sizeof( uint64_t ) );
      break;
#endif /* STORAGE_HASH_FNV */

#ifdef STORAGE_HASH_MURMUR
   case VBHASH_MURMUR:
      hash_temp_32 = hash_file_murmur( file_path );
      memcpy( hash, &hash_temp_32, sizeof( uint32_t ) );
      break;
#endif /* STORAGE_HASH_MURMUR */

#ifdef STORAGE_HASH_SHA256
   case VBHASH_SHA256:
      hash_file_sha256( file_path, hash );
      return;
#endif /* STORAGE_HASH_SHA256 */
   }
}

void archive_free_storage_file( storage_file* object ) {
   if( NULL != object->path ) {
      bdestroy( object->path );
   }
   if( NULL != object->hardlink_path ) {
      bdestroy( object->hardlink_path );
   }
   if( NULL != object->encrypted_filename ){
      bdestroy( object->encrypted_filename );
   }
}

