
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
         "hash_contents BIGINT," 
         "encrypted_filename VARCHAR( 255 )" \
         ");",
      NULL,
      NULL,
      &err_msg
   );

   if( sql_retval ) {
      DBG_ERR( "Error creating database tables: %s\n", err_msg );
      retval = 1;
      goto cleanup;
   }

cleanup:

   if( NULL != err_msg ) {
      sqlite3_free( err_msg );
   }

   if( NULL != db ) {
      sqlite3_close( db );
   }

   return retval;
}

