#ifndef _KEYREGISTRY_H_
#define _KEYREGISTRY_H_

#include <stdint.h>

#define FS_DISABLED 0u
#define FS_ENABLED  1u

/**
 * Feature Switch to enable/disable the possibility of 
 * overwriting values of existing keys
 * possible values:
 *  - FS_ENABLED
 *  - FS_DISABLED (compiled with strict=yes make param)
 *
 * If the macro is not predefined in the makefile, the update is ENABLED
 */
#ifndef FS_ALLOW_UPDATE
    #define KREG_ALLOW_UPDATE FS_DISABLED
#else
    #define KREG_ALLOW_UPDATE FS_ENABLED
#endif

/** Return values of this module */
#define KREG_OK             0u
#define KREG_ERR_REG_OPEN   1u
#define KREG_KEY_EMPTY      2u
#define KREG_KEY_INVALID    3u
#define KREG_KEY_TOO_LONG   4u
#define KREG_KEY_NOT_FOUND  5u
#define KREG_VAL_TOO_LONG   6u
/* ONLY IN STRICT MODE */
#define KREG_KEY_EXISTS     7u

/* key and value length are resctircted for simplicity */
#define KREG_MAX_KEY_LEN    16u
#define KREG_MAX_VAL_LEN    32u

/**
 * return values:
 *  KREG_OK
 *  KREG_ERR_REG_OPEN
 *  KREG_KEY_INVALID
 *  KREG_KEY_TOO_LONG
 *  KREG_VAL_TOO_LONG
 */
uint8_t KREG_ReadRegistryFile( const char* fileName, uint16_t* lineNr, uint16_t* errPos );


/**
 * return velues:
 *  KREG_OK
 *  KREG_KEY_EMPTY
 *  KREG_KEY_INVALID
 *  KREG_KEY_TOO_LONG
 *  KREG_KEY_NOT_FOUND
 */
uint8_t KREG_GetKey( char* str, char** key, char** value, uint16_t* errPos );

/**
 * return values:
 *  KREG_OK
 *  KREG_KEY_INVALID
 *  KREG_KEY_TOO_LONG
 *  KREG_ERR_KEY_EXISTS
 */
uint8_t KREG_PutKey( char* message, char** key, char** value, uint16_t* errPos );

#endif /* _KEYREGISTRY_H_ */