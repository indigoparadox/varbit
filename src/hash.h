
#ifndef HASH_H
#define HASH_H

#include <stdint.h>

#include "bstrlib.h"

#define STORAGE_HASH_FNV 1
#define STORAGE_HASH_MURMUR 1
#define STORAGE_HASH_SHA256 1

#ifdef STORAGE_HASH_MURMUR
#define STORAGE_HASH_BUFFER_SIZE 4
#define STORAGE_HASH_SEED 32
#endif /* STORAGE_HASH_MURMUR */

#ifdef STORAGE_HASH_SHA256
#define SHA256_CHUNK_SIZE 64
#define SHA256_TOTAL_LEN_LEN 8
#endif /* STORAGE_HASH_SHA256 */

/* Assign numbers to hashes so DBs can be compared across versions. */
enum hash_algo {
#ifdef STORAGE_HASH_FNV
   VBHASH_FNV = 1,
#endif /* STORAGE_HASH_FNV */
#ifdef STORAGE_HASH_MURMUR
   VBHASH_MURMUR = 2,
#endif /* STORAGE_HASH_MURMUR */
#ifdef STORAGE_HASH_SHA256
   VBHASH_SHA256 = 3,
#endif /* STORAGE_HASH_SHA256 */
};

uint32_t hash_file_murmur( const bstring file_path );
uint64_t hash_file_fnv( const bstring file_path );

#endif /* HASH_H */

