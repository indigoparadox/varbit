
#include <stdio.h>
#include <bstrlib.h>
#include <unistd.h>

#include "util.h"
#include "storage.h"

int main( int argc, char** argv ) {
   int arg_iter;
   bstring db_path = NULL;
   int retval = 0;

   /* Parse command line arguments. */
   while( ((arg_iter = getopt( argc, argv, "d:" )) != -1) ) {
      switch( arg_iter ) {
         case 'd':
            db_path = bformat( "%s", optarg );
            break;

         case ':':
            DBG_ERR( "Option -%c requires an operand.\n", optopt );
            break;
      }
   }

   /* Ensure sanity. */
   if( NULL == db_path ) {
      DBG_ERR( "No database specified. Aborting.\n" );
      retval = 1;
      goto cleanup;
   }

   storage_ensure_database( db_path );

cleanup:

   if( NULL != db_path ) {
      bdestroy( db_path );
   }

   return retval;
}

