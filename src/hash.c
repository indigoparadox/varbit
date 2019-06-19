
#include "hash.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef STORAGE_HASH_MURMUR

uint32_t hash_file_murmur( const bstring file_path ) {
   FILE* file_handle;
   char buffer[STORAGE_HASH_BUFFER_SIZE] = { 0 };
   /* const uint32_t* buffer_block = (const uint32_t*) buffer; */
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
   char* file_path_c = NULL;

   /* Get file information. */
   file_path_c = bdata( file_path );
   stat_retval = stat( file_path_c, &file_stat );
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

uint64_t hash_file_fnv( const bstring file_path ) {
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

