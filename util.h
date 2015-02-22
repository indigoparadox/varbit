
#ifndef UTIL_H
#define UTIL_H

#define DBG_ERR( ... ) fprintf( stderr, __VA_ARGS__ )

#define CATCH_NONZERO( local_retval, proc_retval, val_arg, ... ) \
   if( local_retval ) { \
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

