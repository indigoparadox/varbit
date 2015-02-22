
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
      "INSERT INTO files (path, mdate, inode, size) VALUES(?, ?, ?, ?)"
   );
   storage_file file_object;
   int object_retval = 0;

   /* Setup the file object. */
   memset( &file_object, 0, sizeof( file_object ) );
   file_object.path = bfromcstr( "" );
   file_object.hardlink_path = bfromcstr( "" );
   file_object.encrypted_filename = bfromcstr( "" );

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
   CATCH_NONZERO( stat_retval, retval, 1, "Unable to prepare SQL statement." );

   sql_retval = sqlite3_bind_text(
      query, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
   );
   CATCH_NONZERO( stat_retval, retval, 1, "Unable to bind SQL parameter." );

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
      if( g_verbose ) {
         printf( "No existing entry found for: %s\n", bdata( file_path ) );
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
         stat_retval, retval, 1, "Unable to prepare SQL statement."
      );

      sql_retval = sqlite3_bind_text(
         insert, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
      );
      CATCH_NONZERO( stat_retval, retval, 1, "Unable to bind SQL parameter." );

      sql_retval = sqlite3_bind_int64( insert, 2, file_stat.st_mtime );
      CATCH_NONZERO( stat_retval, retval, 1, "Unable to bind SQL parameter." );

      sql_retval = sqlite3_bind_int64( insert, 3, file_stat.st_ino );
      CATCH_NONZERO( stat_retval, retval, 1, "Unable to bind SQL parameter." );

      sql_retval = sqlite3_bind_int64( insert, 4, file_stat.st_size );
      CATCH_NONZERO( stat_retval, retval, 1, "Unable to bind SQL parameter." );

      sql_retval = sqlite3_step( insert );

   } else {
      /* TODO: Test inode/date and UPDATE statement. */
   }

cleanup:

   bdestroy( query_string );
   bdestroy( insert_string );
   storage_free_storage_file( &file_object );
   sqlite3_finalize( query );
   if( NULL != insert ) {
      sql_retval = sqlite3_finalize( insert );
      if( sql_retval ) {
         DBG_ERR( "Insert statement failed: %s\n", bdata( file_path ) );
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
         /* TODO: Record file attributes. */
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

int storage_sql_storage_file( sqlite3_stmt* row, storage_file* object ) {
   int retval = 0;

   if( 7 > sqlite3_column_count( row ) ) {
      DBG_ERR( "Unable to convert row: Too few columns.\n" );
      retval = 1;
      goto cleanup;
   }

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

