
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
#include <sys/stat.h>

#include "util.h"

int archive_inventory_update_walk( DB_TYPE db, bstring archive_path ) {
   int retval = 0;
   int bstr_retval = 0;
   DIR* dir = NULL;
   struct dirent* entry = NULL;
   bstring subdir_path = bfromcstr( "" );
   char* archive_path_c = NULL;

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
         archive_inventory_update_walk( db, subdir_path );
         continue;
      } else if( DT_REG == entry->d_type ) {
         /* Record file attributes. */
         db_inventory_update_file( db, subdir_path );
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

#ifdef STORAGE_HASH_MURMUR

uint32_t archive_hash_file( const bstring file_path ) {
   FILE* file_handle;
   char buffer[STORAGE_HASH_BUFFER_SIZE] = { 0 };
   const uint32_t* buffer_block = (const uint32_t*) buffer;
   size_t read_bytes = 0;
   struct stat file_stat;
   int stat_retval = 0;
   static const uint32_t c1 = 0xcc9e2d51;
   static const uint32_t c2 = 0x1b873593;
   static const uint32_t r1 = 15;
   static const uint32_t r2 = 13;
   static const uint32_t m = 5;
   static const uint32_t n = 0xe6546b64;
   uint32_t hash = STORAGE_HASH_SEED;
   int nblocks = 0;
   uint32_t block_iter;
   uint32_t k1 = 0;

   /* Get file information. */
   stat_retval = stat( bdata( file_path ), &file_stat );
   CATCH_NONZERO(
      stat_retval, hash, 0, "Unable to open file: %s\n", bdata( file_path )
   );

   file_handle = fopen( bdata( file_path ), "rb" );
   CATCH_NULL( 
      file_handle, hash, 0, "Unable to open file: %s\n", bdata( file_path )
   );

   do {
      read_bytes = fread(
         buffer, sizeof( char ), STORAGE_HASH_BUFFER_SIZE, file_handle
      );

      /* Hash this block. */
      block_iter *= c1;
      block_iter = (block_iter << r1) | (block_iter >> (32 - r1));
      block_iter *= c2;
 
      hash ^= block_iter;
      hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;

      /* Count this block. (Should come out to filesize / 4.) */
      nblocks++;
   } while( STORAGE_HASH_BUFFER_SIZE == read_bytes );

   /* Hash the tail of the file (remainder of 4). */
   switch( file_stat.st_size & 3 ) {
      case 3:
         k1 ^= buffer[2] << 16;
      case 2:
         k1 ^= buffer[1] << 8;
      case 1:
         k1 ^= buffer[0];
   
         k1 *= c1;
         k1 = (k1 << r1) | (k1 >> (32 - r1));
         k1 *= c2;
         hash ^= k1;
   }
 
   hash ^= file_stat.st_size;
   hash ^= (hash >> 16);
   hash *= 0x85ebca6b;
   hash ^= (hash >> 13);
   hash *= 0xc2b2ae35;
   hash ^= (hash >> 16);

cleanup:

   if( NULL != file_handle ) {
      fclose( file_handle );
   }

   return hash;
}

#endif /* STORAGE_HASH_MURMUR */

#ifdef STORAGE_HASH_FNV

uint64_t archive_hash_file( const bstring file_path ) {
   FILE* file_handle;
   uint8_t buffer[1] = { 0 };
   size_t read_bytes = 0;
   const uint64_t fnv_prime_64 = 1099511628211U;
   const uint64_t fnv_offset_basis_64 = 14695981039346656037U;
   uint64_t hash = fnv_offset_basis_64;

   /* Open file. */
   file_handle = fopen( bdata( file_path ), "rb" );
   CATCH_NULL( 
      file_handle, hash, 0, "Unable to open file: %s\n", bdata( file_path )
   );

   do {
      read_bytes = fread( buffer, sizeof( uint8_t ), 1, file_handle );

      /* Hash this block. */
      hash *= fnv_prime_64;
      hash ^= *buffer;
 
   } while( 1 == read_bytes );

cleanup:

   if( NULL != file_handle ) {
      fclose( file_handle );
   }

   return hash;
}

#endif /* STORAGE_HASH_FNV */

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

