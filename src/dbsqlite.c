
#include "db.h"

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <stdint.h>
#include <inttypes.h>

#include "util.h"
#include "archive.h"

extern int g_verbose;

const struct tagbstring g_temp_inv_query = bsStatic(
   "SELECT * FROM files WHERE path=?" );
const struct tagbstring g_temp_inv_insert = bsStatic(
   "INSERT INTO files (path, mdate, inode, size, hash_contents) "
      "VALUES(?, ?, ?, ?, ?)" );
const struct tagbstring g_temp_inv_update = bsStatic(
   "UPDATE files SET mdate=?, inode=?, size=?, hash_contents=? WHERE path=?" );

int db_ensure_database( sqlite3* db ) {
   int sql_retval = 0;
   int retval = 0;
   char* err_msg = NULL;

   /* Ensure tables exist. */
   /* TODO: Encrypted contents hash? */
   /* TODO: System table with version. */
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

   return retval;
}

static int db_sql_storage_file( sqlite3_stmt* row, storage_file* object ) {
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
         TODO: Encrypted contents hash?
   */

   /* Translate result into storage_file object. */
   bassignformat( object->path, "%s", sqlite3_column_text( row, 0 ) );
   bassignformat( object->hardlink_path, "%s", sqlite3_column_text( row, 1 ) );
   object->mdate = sqlite3_column_int64( row, 2 );
   object->inode = sqlite3_column_int64( row, 3 );
   object->size = sqlite3_column_int64( row, 4 );
   object->hash_contents = sqlite3_column_int64( row, 5 );
   bassignformat( object->encrypted_filename,
      "%s", sqlite3_column_text( row, 6 ) );

cleanup:
   return retval;
}

int db_inventory_update_file( sqlite3* db, bstring file_path ) {
   struct stat file_stat;
   int stat_retval = 0;
   int retval = 0;
   /* char* err_msg = NULL; */
   int existing_found = 0;
   sqlite3_stmt* query = NULL;
   sqlite3_stmt* insert = NULL;
   int sql_retval = 0;
   storage_file file_object;
   int object_retval = 0;
   uint64_t file_hash = 0;
   char* file_path_c = NULL;

   /* Setup the file object. */
   memset( &file_object, 0, sizeof( file_object ) );
   file_object.path = bfromcstr( "" );
   file_object.hardlink_path = bfromcstr( "" );
   file_object.encrypted_filename = bfromcstr( "" );
   file_hash = 0;

   /* Get file information. */
   file_path_c = bdata( file_path );
   stat_retval = stat( file_path_c, &file_stat );
   CATCH_NONZERO(
      stat_retval, retval, 1, "Unable to open file: %s\n", bdata( file_path )
   );

   /* Search for existing entries for file. */
   sql_retval = sqlite3_prepare_v2(
      db,
      (const char*)(g_temp_inv_query.data),
      g_temp_inv_query.slen,
      &query,
      NULL
   );
   CATCH_NONZERO( sql_retval, retval, 1, "Unable to prepare SQL statement.\n" );

   sql_retval = sqlite3_bind_text(
      query, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
   );
   CATCH_NONZERO( sql_retval, retval, 1, "Unable to bind SQL parameter.\n" );

   do {
      sql_retval = sqlite3_step( query );
      switch( sql_retval ) {
         case SQLITE_DONE:
            break;
   
         case SQLITE_ROW:
            existing_found++;
            object_retval = db_sql_storage_file( query, &file_object );
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
      file_hash = archive_hash_file( file_path );

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
         (const char*)(g_temp_inv_insert.data),
         g_temp_inv_insert.slen,
         &insert,
         NULL
      );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to prepare SQL statement.\n"
      );

      sql_retval = sqlite3_bind_text(
         insert, 1, bdata( file_path ), blength( file_path ), SQLITE_STATIC
      );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: path\n"
      );

      sql_retval = sqlite3_bind_int64( insert, 2, file_stat.st_mtime );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: mtime\n"
      );

      sql_retval = sqlite3_bind_int64( insert, 3, file_stat.st_ino );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: inode\n"
      );

      sql_retval = sqlite3_bind_int64( insert, 4, file_stat.st_size );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: size\n"
      );

      sql_retval = sqlite3_bind_int64( insert, 5, file_hash );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: hash\n"
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

         file_hash = archive_hash_file( file_path );
            printf( "New hash: %" PRIu64 "\n", file_hash );

         /* Update the file record. */
         sql_retval = sqlite3_prepare_v2(
            db,
            (const char*)(g_temp_inv_update.data),
            g_temp_inv_update.slen,
            &insert,
            NULL
         );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to prepare SQL statement.\n"
         );

         sql_retval = sqlite3_bind_int64( insert, 1, file_stat.st_mtime );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: mtime.\n"
         );

         sql_retval = sqlite3_bind_int64( insert, 2, file_stat.st_ino );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: inode.\n"
         );

         sql_retval = sqlite3_bind_int64( insert, 3, file_stat.st_size );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: size.\n"
         );

         sql_retval = sqlite3_bind_int64( insert, 4, file_hash );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: hash\n"
         );

         sql_retval = sqlite3_bind_text(
            insert, 5, bdata( file_path ), blength( file_path ), SQLITE_STATIC
         );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: path\n"
         );

         sqlite3_step( insert );
      }
   }

cleanup:

   archive_free_storage_file( &file_object );
   sqlite3_finalize( query );
   if( NULL != insert ) {
      sql_retval = sqlite3_finalize( insert );
      if( sql_retval ) {
         DBG_ERR( "INSERT/UPDATE statement failed: %s\n", bdata( file_path ) );
      }
   }

   return retval;
}

int db_print_dupe( void* arg, int cols, char** strs, char** col_names ) {
   /* TODO: Add to a list of duplicate hashes. */
   printf( "%s\n", strs[0] );
   return 0;
}

void db_list_dupes( sqlite3* db ) {
   char* sql_err = NULL;

   sqlite3_exec(
      db,  
      "select hash_contents from files group by hash_contents "
         "having count(*) > 1;",
      db_print_dupe,
      NULL,
      &sql_err
   );

   
}

