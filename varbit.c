
#include <stdio.h>
#include <bstrlib.h>
#include <unistd.h>

#include "util.h"
#include "storage.h"

int g_verbose = 0;

int main( int argc, char** argv ) {
   int arg_iter;
   bstring db_path = NULL;
   bstring arc_path = NULL;
   int retval = 0;
   int storage_retval = 0;

   /* Parse command line arguments. */
   while( ((arg_iter = getopt( argc, argv, "vd:a:" )) != -1) ) {
      switch( arg_iter ) {
         case 'v':
            g_verbose = 1;
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
   CATCH_NULL( db_path, retval, 1, "No database specified. Aborting.\n" );
   CATCH_NULL( arc_path, retval, 1, "No archive specified. Aborting.\n" );

   /* TODO: Perform operations other than update. */

   storage_retval = storage_ensure_database( db_path );
   CATCH_NONZERO(
      storage_retval, retval, 1, "Error ensuring database tables. Aborting.\n"
   );

   storage_retval = storage_inventory_update_walk( db_path, arc_path );
   CATCH_NONZERO(
      storage_retval, retval, 1, "Error updating inventory. Aborting.\n"
   );

cleanup:

   if( NULL != db_path ) {
      bdestroy( db_path );
   }

   if( NULL != arc_path ) {
      bdestroy( arc_path );
   }

   return retval;
}

