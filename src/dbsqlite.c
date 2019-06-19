
#include "db.h"

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "archive.h"

extern int g_verbose;

const struct tagbstring g_temp_inv_query = bsStatic(
   "SELECT * FROM files WHERE path=?" );
const struct tagbstring g_temp_inv_insert = bsStatic(
   "INSERT INTO files (path, mdate, inode, size, hash_contents, hash_type) "
      "VALUES(?, ?, ?, ?, ?, ?)" );
const struct tagbstring g_temp_inv_update = bsStatic(
   "UPDATE files SET mdate=?, inode=?, size=?, hash_contents=?, hash_type=? "
      "WHERE path=?" );

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
         "hash_contents VARCHAR( 64 )," \
         "hash_type INT NOT NULL," \
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

   if( 8 > sqlite3_column_count( row ) ) {
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
         "hash_contents BLOB," \
         "encrypted_filename VARCHAR( 255 )" \
         TODO: Encrypted contents hash?
   */

   /* Translate result into storage_file object. */
   bassignformat( object->path, "%s", sqlite3_column_text( row, 0 ) );
   bassignformat( object->hardlink_path, "%s", sqlite3_column_text( row, 1 ) );
   object->mdate = sqlite3_column_int64( row, 2 );
   object->inode = sqlite3_column_int64( row, 3 );
   object->size = sqlite3_column_int64( row, 4 );
   bassignformat( object->hash_contents,
      "%s",  sqlite3_column_text( row, 5 ) );
   object->hash_type = sqlite3_column_int64( row, 6 );
   bassignformat( object->encrypted_filename,
      "%s", sqlite3_column_text( row, 7 ) );

cleanup:
   return retval;
}

int db_inventory_update_file(
   sqlite3* db, bstring file_path, enum hash_algo hash_type
) {
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
   char* file_path_c = NULL;
   uint8_t file_hash[HASH_MAX_LEN];
   bstring hash_printable = NULL;

   /* Setup the file object. */
   memset( &file_object, 0, sizeof( file_object ) );
   file_object.path = bfromcstr( "" );
   file_object.hardlink_path = bfromcstr( "" );
   file_object.encrypted_filename = bfromcstr( "" );
   memset( file_hash, '\0', HASH_MAX_LEN );

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
      archive_hash_file( file_path, hash_type, file_hash );

      hash_printable = hash_make_printable( file_hash, hash_type );
      if( g_verbose ) {
         printf(
            "No existing entry found for: %s: %s\n",
            bdata( file_path ),
            bdata( hash_printable )
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

      sql_retval = sqlite3_bind_text(
         insert, 5, bdata( hash_printable ), blength( hash_printable ),
         SQLITE_STATIC );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: hash\n"
      );

      sql_retval = sqlite3_bind_int64( insert, 6, hash_type );
      CATCH_NONZERO(
         sql_retval, retval, 1, "Unable to bind SQL parameter: hash_type\n"
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

         archive_hash_file( file_path, hash_type, file_hash );

         hash_printable = hash_make_printable( file_hash, hash_type );
         printf( "New hash: %s\n", bdata( hash_printable ) );

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

         sql_retval = sqlite3_bind_text(
            insert, 4, bdata( hash_printable ), blength( hash_printable ),
            SQLITE_STATIC );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: hash\n"
         );

         sql_retval = sqlite3_bind_int64( insert, 5, hash_type );
         CATCH_NONZERO(
            sql_retval, retval, 1, "Unable to bind SQL parameter: hash_type\n"
         );

         sql_retval = sqlite3_bind_text(
            insert, 6, bdata( file_path ), blength( file_path ), SQLITE_STATIC
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
   bdestroy( hash_printable );

   return retval;
}

int db_print_dupe( void* arg, int cols, char** strs, char** col_names ) {
   struct db_hash_list* hash_list = (struct db_hash_list*)arg;
   int new_sz = 0;
   int retval = 0;
   
   if( NULL == hash_list->list ) {
      hash_list->list = calloc( 6, sizeof( bstring ) );
      if( NULL != hash_list->list ) {
         hash_list->sz = 6;
      } else {
         retval = -1;
         goto cleanup;
      }
   } else if( hash_list->len + 1 >= hash_list->sz ) {
      new_sz = hash_list->sz * 2;
      hash_list->list =
         realloc( hash_list->list, new_sz * sizeof( bstring ) );
      if( NULL != hash_list ) {
         hash_list->sz = new_sz;
      } else {
         hash_list->sz = 0;
         retval = -1;
         goto cleanup;
      }
   }

   hash_list->list[hash_list->len] = bfromcstr( strs[0] );
   hash_list->len++;

cleanup:

   return retval;
}

int db_list_dupes( sqlite3* db, struct db_hash_list* hash_list ) {
   char* err_msg = NULL;
   int count = 0;

   sqlite3_exec(
      db,  
      "select hash_contents from files group by hash_contents "
         "having count(*) > 1;",
      db_print_dupe,
      hash_list,
      &err_msg
   );
   
   if( NULL != err_msg ) {
      sqlite3_free( err_msg );
   }

   return count;
}

