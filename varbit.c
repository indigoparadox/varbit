
/* This file is part of varbit.
 * varbit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * varbit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with varbit.  If not, see <http://www.gnu.org/licenses/>.
 */

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

