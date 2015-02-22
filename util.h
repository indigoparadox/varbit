
#ifndef UTIL_H
#define UTIL_H

#define DBG_ERR( ... ) fprintf( stderr, __VA_ARGS__ )

#define CATCH( retval, val_arg, ... ) \
   if( retval ) { \
      DBG_ERR( __VA_ARGS__ ); \
      retval = val_arg; \
      goto cleanup; \
   }

#endif /* UTIL_H */

