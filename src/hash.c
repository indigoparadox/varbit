
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

#ifdef STORAGE_HASH_SHA256

/*
 * Initialize array of round constants:
 * (first 32 bits of the fractional parts of the cube roots of the first 64
 * primes 2..311):
 */
static const uint32_t k[] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
   0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
   0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
   0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
   0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
   0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
   0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
   0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
   0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct buffer_state {
   const uint8_t * p;
   size_t len;
   size_t total_len;
   int single_one_delivered; /* bool */
   int total_len_delivered; /* bool */
};

static inline uint32_t right_rot( uint32_t value, unsigned int count ) {
   /*
    * Defined behaviour in standard C for all count where 0 < count < 32,
    * which is what we need here.
    */
   return value >> count | value << (32 - count);
}

static void init_buf_state(
   struct buffer_state * state, const void * input, size_t len
) {
   state->p = input;
   state->len = len;
   state->total_len = len;
   state->single_one_delivered = 0;
   state->total_len_delivered = 0;
}

/* Return value: bool */
static int calc_sha256_chunk(
   uint8_t chunk[SHA256_CHUNK_SIZE], struct buffer_state * state
) {
   size_t space_in_chunk;

   if (state->total_len_delivered) {
      return 0;
   }

   if (state->len >= SHA256_CHUNK_SIZE) {
      memcpy(chunk, state->p, SHA256_CHUNK_SIZE);
      state->p += SHA256_CHUNK_SIZE;
      state->len -= SHA256_CHUNK_SIZE;
      return 1;
   }

   memcpy(chunk, state->p, state->len);
   chunk += state->len;
   space_in_chunk = SHA256_CHUNK_SIZE - state->len;
   state->p += state->len;
   state->len = 0;

   /* If we are here, space_in_chunk is one at minimum. */
   if (!state->single_one_delivered) {
      *chunk++ = 0x80;
      space_in_chunk -= 1;
      state->single_one_delivered = 1;
   }

   /*
    * Now:
    * - either there is enough space left for the total length, and we can
    *   conclude,
    * - or there is too little space left, and we have to pad the rest of this
    *   chunk with zeroes.
    * In the latter case, we will conclude at the next invokation of this
    * function.
    */
   if (space_in_chunk >= SHA256_TOTAL_LEN_LEN) {
      const size_t left = space_in_chunk - SHA256_TOTAL_LEN_LEN;
      size_t len = state->total_len;
      int i;
      memset(chunk, 0x00, left);
      chunk += left;

      /* Storing of len * 8 as a big endian 64-bit without overflow. */
      chunk[7] = (uint8_t) (len << 3);
      len >>= 5;
      for (i = 6; i >= 0; i--) {
         chunk[i] = (uint8_t) len;
         len >>= 8;
      }
      state->total_len_delivered = 1;
   } else {
      memset(chunk, 0x00, space_in_chunk);
   }

   return 1;
}

/*
 * Limitations:
 * - Since input is a pointer in RAM, the data to hash should be in RAM, which
 *   could be a problem
 *   for large data sizes.
 * - SHA algorithms theoretically operate on bit strings. However, this
 *   implementation has no support
 *   for bit string lengths that are not multiples of eight, and it really
 *   operates on arrays of bytes.
 *   In particular, the len parameter is a number of bytes.
 */
void calc_sha256( uint8_t hash[32], const void* input, size_t len ) {
   uint32_t w[64];
   const uint8_t* p = NULL;
   uint32_t ah[8];
   uint32_t s0, s1, temp1, temp2, maj, ch;

   /*
    * Note 1: All integers (expect indexes) are 32-bit unsigned integers and
    *         addition is calculated modulo 2^32.
    * Note 2: For each round, there is one round constant k[i] and one entry in
    *         the message schedule array w[i], 0 = i = 63
    * Note 3: The compression function uses 8 working variables, a through h
    * Note 4: Big-endian convention is used when expressing the constants in
    *     this pseudocode,
    *     and when parsing message block data from bytes to words, for example,
    *     the first word of the input message "abc" after padding is 0x61626380
    */

   /*
    * Initialize hash values:
    * (first 32 bits of the fractional parts of the square roots of the first 8
    * primes 2..19):
    */
   uint32_t h[] = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c,
       0x1f83d9ab, 0x5be0cd19
   };
   int i, j;

   /* 512-bit chunks is what we will operate on. */
   uint8_t chunk[64];

   struct buffer_state state;

   init_buf_state( &state, input, len );

   while( calc_sha256_chunk( chunk, &state ) ) {
      memset( ah, '\0', sizeof( ah ) );
      
      /*
       * create a 64-entry message schedule array w[0..63] of 32-bit words
       * (The initial values in w[0..63] don't matter, so many implementations
       * zero them here)
       * copy chunk into first 16 words w[0..15] of the message schedule array
       */
      p = chunk;

      memset( w, 0x00, sizeof w );
      for( i = 0; i < 16; i++ ) {
         w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
            (uint32_t)p[2] << 8 | (uint32_t)p[3];
         p += 4;
      }

      /* Extend the first 16 words into the remaining 48 words w[16..63] of 
       * the message schedule array: */
      for( i = 16 ; i < 64 ; i++ ) {
         s0 = right_rot( w[i - 15], 7) ^ 
            right_rot( w[i - 15], 18 ) ^ (w[i - 15] >> 3);
         s1 = right_rot( w[i - 2], 17 ) ^
            right_rot( w[i - 2], 19 ) ^ (w[i - 2] >> 10);
         w[i] = w[i - 16] + s0 + w[i - 7] + s1;
      }
      
      /* Initialize working variables to current hash value: */
      for( i = 0 ; i < 8 ; i++ ) {
         ah[i] = h[i];
      }

      /* Compression function main loop: */
      for( i = 0 ; i < 64 ; i++ ) {
         s1 = right_rot(ah[4], 6) ^ right_rot(ah[4], 11) ^ right_rot(ah[4], 25);
         ch = (ah[4] & ah[5]) ^ (~ah[4] & ah[6]);
         temp1 = ah[7] + s1 + ch + k[i] + w[i];
         s0 = right_rot(ah[0], 2) ^ right_rot(ah[0], 13) ^ right_rot(ah[0], 22);
         maj = (ah[0] & ah[1]) ^ (ah[0] & ah[2]) ^ (ah[1] & ah[2]);
         temp2 = s0 + maj;

         ah[7] = ah[6];
         ah[6] = ah[5];
         ah[5] = ah[4];
         ah[4] = ah[3] + temp1;
         ah[3] = ah[2];
         ah[2] = ah[1];
         ah[1] = ah[0];
         ah[0] = temp1 + temp2;
      }

      /* Add the compressed chunk to the current hash value: */
      for( i = 0 ; i < 8 ; i++ ) {
         h[i] += ah[i];
      }
   }

   /* Produce the final hash value (big-endian): */
   for( i = 0, j = 0 ; i < 8 ; i++ ) {
      hash[j++] = (uint8_t)(h[i] >> 24);
      hash[j++] = (uint8_t)(h[i] >> 16);
      hash[j++] = (uint8_t)(h[i] >> 8);
      hash[j++] = (uint8_t)h[i];
   }
}

int hash_file_sha256( const bstring file_path, uint8_t hash[32] ) {
   FILE* file_handle;
   uint8_t buffer[1] = { 0 };
   size_t read_bytes = 0;
   int retval = 0;

   /* Open file. */
   file_handle = fopen( bdata( file_path ), "rb" );
   CATCH_NULL( 
      file_handle, retval, 0, "Unable to open file: %s\n", bdata( file_path )
   );

   do {
      read_bytes = fread( buffer, sizeof( uint8_t ), 1, file_handle );

      calc_sha256( hash, buffer, 1 );
 
   } while( 1 == read_bytes );

cleanup:

   if( NULL != file_handle ) {
      fclose( file_handle );
   }

   return retval;
}

#endif /* STORAGE_HASH_SHA256 */

bstring hash_make_printable( uint8_t hash[HASH_MAX_LEN], enum hash_algo type ) {
   bstring out = NULL;
   bstring btmpchar = NULL;
   int i = 0;
   int hash_len = 0;
   uint8_t* cp = NULL;

   switch( type ) {
   case VBHASH_FNV:
      hash_len = sizeof( uint64_t );
      break;
   case VBHASH_MURMUR:
      hash_len = sizeof( uint32_t );
      break;
   case VBHASH_SHA256:
      hash_len = 32;
      break;
   }

   out = bfromcstr( "" );
   btmpchar = bfromcstr( "" );
   for( i = 0 ; hash_len > i ; i++ ) {
      cp = &(hash[i]);
      bassignformat( btmpchar, "%x", *cp );
      bconcat( out, btmpchar );
   }

   return out;
}

