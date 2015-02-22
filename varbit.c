
#include <stdio.h>
#include <bstrlib.h>
#include <unistd.h>

#include "util.h"
#include "storage.h"

int main( int argc, char** argv ) {
   int arg_iter;
   bstring db_path = NULL;
   bstring arc_path = NULL;
   int retval = 0;
   int verbose = 0;

   /* Parse command line arguments. */
   while( ((arg_iter = getopt( argc, argv, "vd:a:" )) != -1) ) {
      switch( arg_iter ) {
         case 'v':
            verbose = 1;
            break;

         case 'd':
            db_path = bformat( "%s", optarg );
            break;

         case 'a':
            arc_path = bformat( "%s", optarg );
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

   if( NULL == arc_path ) {
      DBG_ERR( "No archive specified. Aborting.\n" );
      retval = 1;
      goto cleanup;
   }

   if( storage_ensure_database( db_path ) ) {
      DBG_ERR( "Error ensuring database tables. Aborting.\n" );
      retval = 1;
      goto cleanup;
   }

   if( storage_inventory_update( db_path, arc_path ) ) {
      DBG_ERR( "Error updating inventory. Aborting.\n" );
      retval = 1;
      goto cleanup;
   }

cleanup:

   if( NULL != db_path ) {
      bdestroy( db_path );
   }

   if( NULL != arc_path ) {
      bdestroy( arc_path );
   }

   return retval;
}

