
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
int g_force = 0;

int main( int argc, char** argv ) {
   int arg_iter;
   bstring db_path = NULL;
   bstring arc_path = NULL;
   int retval = 0;
   int storage_retval = 0;
   int scan = 0;
   int dedup = 0;
   int prune = 0;
   storage_group* dupe_file_groups = NULL;

   /* Parse command line arguments. */
   while( ((arg_iter = getopt( argc, argv, "hvfd:a:slp" )) != -1) ) {
      switch( arg_iter ) {
         case 'h':

            printf( "Usage: varbit -d <db_path> -a <arc_path> [-v] " );
            printf( "[-f] [-s|-l|-p]\n" );
            printf( "This tool will build a database of hashes for all\n" );
            printf( "files in the specified archive directory.\n" );
            printf( "\n" );
            printf( "-s\t\tScan arc_path for new files.\n" );
            printf( "-l\t\tScan the database for duplicate files.\n" );
            printf( "-p\t\tPrune deleted files from the database.\n" );
            printf( "-d <db_path>\tThe location of the files database.\n" );
            printf( "-a <arc_path>\tThe directory of the file archive to " \
               "catalog.\n" );
            printf( "-v\t\tBe verbose.\n" );
            printf( "-f\t\tForce. Don't ask questions. (DANGEROUS!)\n" );
            printf( "\n" );
            printf( "The database path is always required.\n" );
            printf( "The archive path is required for (s)can.\n" );
         
            /* Exit immediately after showing help. */
            goto cleanup;

         case 'v':
            g_verbose = 1;
            break;

         case 'f':
            g_force = 1;
            break;

         case 'd':
            db_path = bformat( "%s", optarg );
            break;

         case 'a':
            arc_path = bformat( "%s", optarg );
            break;

         case 's':
            scan = 1;
            break;

         case 'l':
            dedup = 1;
            break;

         case 'p':
            prune = 1;
            break;

         case ':':
            DBG_ERR( "Option -%c requires an operand.\n", optopt );
            break;
      }
   }

   /* Ensure sanity. */
   CATCH_NULL( db_path, retval, 1, "No database specified. Aborting.\n" );
   if( scan ) {
      CATCH_NULL( arc_path, retval, 1, "No archive specified. Aborting.\n" );
   }

   /* TODO: Perform operations other than update. */

   if( scan ) {
      storage_retval = storage_ensure_database( db_path );
      CATCH_NONZERO(
         storage_retval, retval, 1,
         "Error ensuring database tables. Aborting.\n"
      );

      storage_retval = storage_inventory_update_walk( db_path, arc_path );
      CATCH_NONZERO(
         storage_retval, retval, 1, "Error updating inventory. Aborting.\n"
      );
   }

   if( dedup ) {
      storage_retval =
         storage_inventory_dedupe_find( db_path, &dupe_file_groups );
      CATCH_NONZERO(
         storage_retval, retval, 1, "Error searching for dupes. Aborting. \n"
      );
   }

   if( prune ) {
      /* TODO: If an archive path was specified, limit pruning to that path
       *       using LIKE for the query.
       */
      storage_retval = storage_inventory_prune( db_path );
      CATCH_NONZERO(
         storage_retval, retval, 1, "Error pruning. Aborting. \n"
      );
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

