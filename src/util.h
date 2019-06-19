
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

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

#define DBG_ERR( ... ) fprintf( stderr, __VA_ARGS__ )

#define CATCH_NONZERO( local_retval, proc_retval, val_arg, ... ) \
   if( local_retval ) { \
      DBG_ERR( __VA_ARGS__ ); \
      proc_retval = val_arg; \
      goto cleanup; \
   }

#define CATCH_ZERO( local_retval, proc_retval, val_arg, ... ) \
   if( !local_retval ) { \
      DBG_ERR( __VA_ARGS__ ); \
      proc_retval = val_arg; \
      goto cleanup; \
   }

#define CATCH_NULL( local_retval, proc_retval, val_arg, ... ) \
   if( NULL == local_retval ) { \
      DBG_ERR( __VA_ARGS__ ); \
      proc_retval = val_arg; \
      goto cleanup; \
   }

#define CATCH_BSTRERR( local_retval, proc_retval, val_arg ) \
   if( BSTR_ERR == local_retval ) { \
      DBG_ERR( "Unable to assign bstring. Aborting.\n" ); \
      proc_retval = val_arg; \
      goto cleanup; \
   }

#endif /* UTIL_H */

