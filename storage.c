
#include "storage.h"

/* End MurmurHash. */

int storage_ensure_database( bstring db_path ) {
   sqlite3* db = NULL;
   int sql_retval = 0;
   int retval = 0;
   char* err_msg = NULL;

   sql_retval = sqlite3_open( bdata( db_path ), &db );

   if( sql_retval ) {
      DBG_ERR( "Unable to open database: %s\n", bdata( db_path ) );
      retval = 1;
      goto cleanup;
   }

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
   CATCH( sql_retval, 1, "Error creating database tables: %s\n", err_msg );

   sql_retval = sqlite3_exec(
      db,
      "CREATE INDEX IF NOT EXISTS hardlink_path_idx ON files( hardlink_path )",
      NULL,
      NULL,
      &err_msg
   );
   CATCH( sql_retval, 1, "Error creating database index: %s\n", err_msg );

   sql_retval = sqlite3_exec(
      db,
      "CREATE INDEX IF NOT EXISTS encrypted_filename_idx " \
         "ON files( encrypted_filename )",
      NULL,
      NULL,
      &err_msg
   );
   CATCH( sql_retval, 1, "Error creating database index: %s\n", err_msg );

cleanup:

   if( NULL != err_msg ) {
      sqlite3_free( err_msg );
   }

   if( NULL != db ) {
      sqlite3_close( db );
   }

   return retval;
}

int storage_inventory_update( bstring db_path, bstring archive_path ) {
   int retval = 0;


cleanup:

   return retval;
}

