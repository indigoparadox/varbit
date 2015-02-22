
#include "storage.h"

extern int g_verbose;

/* End MurmurHash. */

int storage_ensure_database( bstring db_path ) {
   sqlite3* db = NULL;
   int sql_retval = 0;
   int retval = 0;
   char* err_msg = NULL;

   sql_retval = sqlite3_open( bdata( db_path ), &db );
   CATCH_NONZERO(
      sql_retval, retval, 1, "Unable to open database: %s\n", bdata( db_path )
   );

   /* Ensure tables exist. */
   sql_retval = sqlite3_exec(
      db,
      "CREATE TABLE IF NOT EXISTS files(" \
         "path VARCHAR( 255 ) PRIMARY KEY," \
         "hardlink_path VARCHAR( 255 )," \
         "mdate INT NOT NULL," \
         "inode INT NOT NULL," \
         "size INT NOT NULL, " \
         "hash_contents BIGINT," \
         "encrypted_filename VARCHAR( 255 )" \
         ");",
      NULL,
      NULL,
      &err_msg
   );
   CATCH_NONZERO(
      sql_retval, retval, 1, "Error creating database tables: %s\n", err_msg
   );

   sql_retval = sqlite3_exec(
      db,
      "CREATE INDEX IF NOT EXISTS hardlink_path_idx ON files( hardlink_path )",
      NULL,
      NULL,
      &err_msg
   );
   CATCH_NONZERO(
      sql_retval, retval, 1, "Error creating database index: %s\n", err_msg
   );

   sql_retval = sqlite3_exec(
      db,
      "CREATE INDEX IF NOT EXISTS encrypted_filename_idx " \
         "ON files( encrypted_filename )",
      NULL,
      NULL,
      &err_msg
   );
   CATCH_NONZERO(
      sql_retval, retval, 1, "Error creating database index: %s\n", err_msg
   );

cleanup:

   if( NULL != err_msg ) {
      sqlite3_free( err_msg );
   }

   if( NULL != db ) {
      sqlite3_close( db );
   }

   return retval;
}

int storage_inventory_update_file( sqlite3* db, bstring file_path ) {
   struct stat file_stat;
   int stat_retval = 0;
   int retval = 0;
   char* err_msg = NULL;
   int existing_found = 0;
   sqlite3_stmt* query;
   sqlite3_stmt* insert = NULL;
   int sql_retval = 0;
   bstring query_string = bfromcstr( "SELECT * FROM files WHERE path=?" );
   bstring insert_string = bfromcstr(
      "INSERT INTO files (path, mdate, inode, size, hash_contents) " \
         "VALUES(?, ?, ?, ?, ?)"
   );
   bstring update_string = bfromcstr(
      "UPDATE files SET mdate=?, inode=?, size=?, hash_contents=? WHERE path=?"
   );
   storage_file file_object;
   int object_retval = 0;
   uint64_t file_hash = 0;

   /* Setup the file object. */
   memset( &file_object, 0, sizeof( file_object ) );
   file_object.path = bfromcstr( "" );
   file_object.hardlink_path = bfromcstr( "" );
   file_object.encrypted_filename = bfromcstr( "" );
   file_hash = 0;

   /* Get file information. */
   stat_retval = stat( bdata( file_path ), &file_stat );
   CATCH_NONZERO(
      stat_retval, retval, 1, "Unable to open file: %s\n", bdata( file_path )
   );

   /* Search for existing entries for file. */
   sql_retval = sqlite3_prepare_v2(
      db,
      bdata( query_string ),
      blength( query_string ),
      &query,
      NULL
   );
   CATCH_NONZERO( sql_retval, retval, 1, "Unable to prepare SQL statement." );

   sql_retval = sqlite3_bind_text(
      query, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
   );
   CATCH_NONZERO( sql_retval, retval, 1, "Unable to bind SQL parameter." );

   do {
      sql_retval = sqlite3_step( query );
      switch( sql_retval ) {
         case SQLITE_DONE:
            break;
   
         case SQLITE_ROW:
            existing_found++;
            object_retval = storage_sql_storage_file( query, &file_object );
            CATCH_NONZERO(
               object_retval, retval, 1,
               "Skipping. Error converting: %s\n", bdata( file_path )
            );
            break;
            
      }
   } while( SQLITE_ROW == sql_retval );

   if( 1 < existing_found ) {
      DBG_ERR(
         "Skipping. Multiple entries found for: %s\n", bdata( file_path )
      );
      retval = 1;
      goto cleanup;
   } else if( 1 > existing_found ) {
      file_hash = storage_hash_file( file_path );

      if( g_verbose ) {
         printf(
            "No existing entry found for: %s: %" PRIu64 "\n",
            bdata( file_path ),
            file_hash
         );
      }

      /* Insert the file record. */
      sql_retval = sqlite3_prepare_v2(
         db,
         bdata( insert_string ),
         blength( insert_string ),
         &insert,
         NULL
      );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to prepare SQL statement."
      );

      sql_retval = sqlite3_bind_text(
         insert, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
      );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: path"
      );

      sql_retval = sqlite3_bind_int64( insert, 2, file_stat.st_mtime );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: mtime"
      );

      sql_retval = sqlite3_bind_int64( insert, 3, file_stat.st_ino );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: inode"
      );

      sql_retval = sqlite3_bind_int64( insert, 4, file_stat.st_size );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: size"
      );

      sql_retval = sqlite3_bind_int64( insert, 5, file_hash );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: hash"
      );

      sqlite3_step( insert );

   } else {
      /* Test inode/date and test hash if different. */
      if(
         file_stat.st_mtime != file_object.mdate ||
         file_stat.st_ino != file_object.inode ||
         file_stat.st_size != file_object.size
      ) {
         if( g_verbose ) {
            printf(
               "FS attributes different, file may have changed: %s\n",
               bdata( file_path )
            );
         }

         file_hash = storage_hash_file( file_path );
            printf( "New hash: %" PRIu64 "\n", file_hash );

         /* Update the file record. */
         sql_retval = sqlite3_prepare_v2(
            db,
            bdata( update_string ),
            blength( update_string ),
            &insert,
            NULL
         );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to prepare SQL statement."
         );

         sql_retval = sqlite3_bind_int64( insert, 1, file_stat.st_mtime );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: mtime."
         );

         sql_retval = sqlite3_bind_int64( insert, 2, file_stat.st_ino );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: inode."
         );

         sql_retval = sqlite3_bind_int64( insert, 3, file_stat.st_size );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: size."
         );

         sql_retval = sqlite3_bind_int64( insert, 4, file_hash );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: hash"
         );

         sql_retval = sqlite3_bind_text(
            insert, 5, bdata( file_path ), blength( file_path ), SQLITE_STATIC
         );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: path"
         );

         sqlite3_step( insert );
      }
   }

cleanup:

   bdestroy( query_string );
   bdestroy( insert_string );
   storage_free_storage_file( &file_object );
   sqlite3_finalize( query );
   if( NULL != insert ) {
      sql_retval = sqlite3_finalize( insert );
      if( sql_retval ) {
         DBG_ERR( "INSERT/UPDATE statement failed: %s\n", bdata( file_path ) );
      }
   }

   return retval;
}

int storage_inventory_update_walk( bstring db_path, bstring archive_path ) {
   int retval = 0;
   int bstr_retval = 0;
   DIR* dir = NULL;
   struct dirent* entry = NULL;
   bstring subdir_path = bfromcstr( "" );
   sqlite3* db = NULL;
   int sql_retval = 0;

   dir = opendir( bdata( archive_path ) );
   CATCH_NULL(
      dir, retval, 1, "Unable to open directory: %s\n", bdata( archive_path )
   );

   sql_retval = sqlite3_open( bdata( db_path ), &db );
   CATCH_NONZERO(
      sql_retval, retval, 1, "Unable to open database: %s\n", bdata( db_path )
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
         storage_inventory_update_walk( db_path, subdir_path );
         continue;
      } else if( DT_REG == entry->d_type ) {
         /* Record file attributes. */
         storage_inventory_update_file( db, subdir_path );
      } else if( DT_LNK == entry->d_type ) {
         /* TODO: Record link existence. */
      }
   }

cleanup:

   bdestroy( subdir_path );

   if( NULL != db ) {
      sqlite3_close( db );
   }

   return retval;
}

#ifdef STORAGE_HASH_MURMUR

uint32_t storage_hash_file( bstring file_path ) {
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

uint64_t storage_hash_file( bstring file_path ) {
   FILE* file_handle;
   uint8_t buffer[1] = { 0 };
   size_t read_bytes = 0;
   const uint64_t fnv_prime_64 = 1099511628211;
   const uint64_t fnv_offset_basis_64 = 14695981039346656037;
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

int storage_sql_storage_file( sqlite3_stmt* row, storage_file* object ) {
   int retval = 0;

   if( 7 > sqlite3_column_count( row ) ) {
      DBG_ERR( "Unable to convert row: Too few columns.\n" );
      retval = 1;
      goto cleanup;
   }

   /*
         "path VARCHAR( 255 ) PRIMARY KEY," \
         "hardlink_path VARCHAR( 255 )," \
         "mdate INT NOT NULL," \
         "inode INT NOT NULL," \
         "size INT NOT NULL, " \
         "hash_contents BIGINT," \
         "encrypted_filename VARCHAR( 255 )" \
   */

   object->path = bformat( "%s", sqlite3_column_text( row, 0 ) );
   object->hardlink_path = bformat( "%s", sqlite3_column_text( row, 1 ) );
   object->mdate = sqlite3_column_int64( row, 2 );
   object->inode = sqlite3_column_int64( row, 3 );
   object->size = sqlite3_column_int64( row, 4 );
   object->hash_contents = sqlite3_column_int64( row, 5 );
   object->encrypted_filename = bformat( "%s", sqlite3_column_text( row, 6 ) );

cleanup:
   return retval;
}

void storage_free_storage_file( storage_file* object ) {
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

