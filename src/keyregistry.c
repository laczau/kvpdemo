#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "keyregistry.h"

/**************************************************************/
/* ------------------- type declarations -------------------- */
/**************************************************************/

/**
 * linked list element type definition to store key-value pairs
 */
typedef struct KeyValuePair_TAG
{
    char* key;
    char* value;
    struct KeyValuePair_TAG* next;
} KeyValuePair;

/**************************************************************/
/* ------------------- module local variables --------------- */
/**************************************************************/

static KeyValuePair* keyValuePairs = NULL;
static KeyValuePair* lastKey = NULL;
static FILE *regFile = NULL;

/**************************************************************/
/* ------------------- function prototypes ------------------ */
/**************************************************************/

static KeyValuePair* searchKey( const char* key );
static uint8_t readKey( const char* key, char** value );
static uint8_t storeKey( char* key, char* value );
static uint8_t parseKeyValue( char** key, char** value, char* line, size_t len, uint16_t* errPos );

/**************************************************************/
/* ------------------- local functions ---------------------- */
/**************************************************************/

/**
 * @brief Returns a kvp entry in the list if the given key exists
 *
 * @param[in]  key
 * @return     kvp if it exists in the list
 *             NULL otherwise
 */
static KeyValuePair* searchKey( const char* key )
{
    KeyValuePair* iter = keyValuePairs;

    while (iter)
    {
        if (strcmp(key, iter->key) == 0)
        {
            return iter;
        }
        iter = iter->next;
    }
    
    return NULL;
}

/**
 * @brief Retrieves the key's value from the list
 *
 * @param[in]  key
 * @param[out] value
 * @return     KREG_OK
 *             KREG_KEY_NOT_FOUND
 */
static uint8_t readKey( const char* key, char** value )
{
    KeyValuePair* keyValue;
    
    if ((keyValue = searchKey(key)) == NULL)
    {
        return KREG_KEY_NOT_FOUND;
    }
    else
    {
        *value = keyValue->value;
    }
    
    return KREG_OK;
}

/**
 * @brief Saves the kvp in the linked list
 *
 * @param[in]  key
 * @param[in]  value
 * @return     KREG_OK
 *             KREG_KEY_EXISTS (only in strict mode)
 */
static uint8_t storeKey( char* key, char* value )
{
    KeyValuePair* newKey;
    if ((newKey = searchKey(key)) != NULL)
    {
#if (KREG_ALLOW_UPDATE == FS_DISABLED)
        return KREG_KEY_EXISTS;
#else
        /* overwrite value */
        newKey->value = value;
#endif
    }
    else
    {
        newKey = (KeyValuePair*) malloc(sizeof(KeyValuePair));
        
        if (newKey != NULL)
        {
            newKey->key = key;
            newKey->value = value;
            newKey->next = NULL;
            
            if (keyValuePairs == NULL)
            {
                keyValuePairs = newKey;
            }
            else
            {
                lastKey->next = newKey;
            }
            
            lastKey = newKey;
        }
    }

    return KREG_OK;
}

/**
 * @brief Extracts the key and value from the given string
 *
 * Parses the input string for the key and value.
 * The format can be expressed with two regular expressions
 * (length limitations are not considered):
 *
 *  1. key with empty value : ^\s*([A-Za-z0-9]+)\s?\r?\n?$
 *  2. key with value       : ^\s*([A-Za-z0-9]+)\s(.*)\r?\n?$
 *
 * If the parse was successful, it allocates memory for the key
 * and optionally for the value (if it is not empty)
 *
 * @param[out]  key
 * @param[out]  value
 * @param[in]   line the input string
 * @param[in]   len length of the input string
 * @param[out]  errPos position of the character where the parse failed
 * @return      KREG_OK
 *              KREG_KEY_INVALID
 *              KREG_KEY_EMPTY
 *              KREG_KEY_TOO_LONG
 *              KREG_VAL_TOO_LONG
 */
static uint8_t parseKeyValue( char** key, char** value, char* line, size_t len, uint16_t* errPos )
{
    /* variables to store key and value */
    char* _key = NULL;
    char* _value = NULL;
    
    /* string iterator */
    char* linePtr = line;

    /* remove leading spaces */
    while(*linePtr == ' ')
    {
        linePtr++;
        len--;
    }

    /* remove trailing (\r)\n chars */
    for (int i = 0; i < 2; i++)
    {
        if ((linePtr[len - 1] == '\n') || (linePtr[len - 1] == '\r'))
        {
            linePtr[len - 1] = '\0';
            len--;
        }
    }

    /* start position of the key */
    char* start = linePtr;
    uint16_t keyLen = 0;

    /* key parser */
    while (1)
    {
        /* key can contain only digits and letters */
        if (isalpha(*linePtr) || isdigit(*linePtr))
        {
            keyLen++;

            /* if length exceeded the maximum allowed length, return */
            if (keyLen > KREG_MAX_KEY_LEN)
            {
                *errPos = linePtr - line + 1;
                return KREG_KEY_TOO_LONG;
            }
        }
        /* key is parsed, because the first space is the separator
         * first character can't be a space, because it was removed before
         */
        else if (*linePtr == ' ')
        {
            /* allocate memory for the key */
            _key = malloc(keyLen);
            memcpy(_key, start, keyLen);
            *key = _key;
            
            /* exit the loop and continiue with value parsing */
            break;
        }
        /* terminal character is reached, this is a key without a value */
        else if (*linePtr == '\0')
        {
            /* if this is the first char, then no key has been provided */
            if (keyLen == 0)
            {
                *errPos = linePtr - line + 1;
                return KREG_KEY_EMPTY;
            }
            
            _key = malloc(keyLen);
            memcpy(_key, start, keyLen);
            *key = _key;
            
            return KREG_OK;
        }
        /* invalid character found in key */
        else
        {
            *errPos = linePtr - line + 1;
            return KREG_KEY_INVALID;
        }
        
        /* iterate the position to the next character */
        linePtr++;
    }

    /* skip space char that separates key and value */
    linePtr++;

    uint8_t valLen = len - (linePtr - start);
    
    if (valLen > KREG_MAX_VAL_LEN)
    {
        *errPos = linePtr - line + KREG_MAX_VAL_LEN + 1;
        return KREG_VAL_TOO_LONG;
    }
    else if (valLen != 0)
    {
        _value = malloc(valLen);
        memcpy(_value, linePtr, valLen);
        *value = _value;
    }
    else /* value length is 0 */
    {
        /* do nothing */
    }

    return KREG_OK;
}

/**************************************************************/
/* ------------------- module interfaces -------------------- */
/**************************************************************/

/**
 * @brief Reads the the registry file
 *
 * Loads the registry file from the storage and builds up the
 * KVP linked list. In case of error, the caller is reported
 * about the position where the parse failed.
 *
 * @param[in]  fileName name of the registry file
 * @param[out] line number where the parse fails
 * @param[out] errPos position of the character in the current line where the parse fails
 * @return     KREG_OK
 *             KREG_ERR_REG_OPEN
 *             KREG_KEY_INVALID
 *             KREG_KEY_TOO_LONG
 *             KREG_VAL_TOO_LONG
 */
uint8_t KREG_ReadRegistryFile( const char* fileName, uint16_t* lineNr, uint16_t* errPos )
{
    char *line = NULL;
    size_t len = 0;
    ssize_t nread = 0;
    uint16_t lineCnt = 0;
    
    regFile = fopen(fileName, "r");
    
    if (regFile == NULL)
    {
        return KREG_ERR_REG_OPEN;
    }
    
    /* read the whole file line by line till EOF */
    while ((nread = getline(&line, &len, regFile)) != -1)
    {
        lineCnt++;
        /* check empty line */
        if ((*line != '\r') && (*line != '\n'))
        {
            char* key = NULL;
            char* value = NULL;
            uint8_t retVal;
            
            if ((retVal = parseKeyValue(&key, &value, line, nread, errPos)) != KREG_OK)
            {        
                /* release resources first */
                free(line);
                fclose(regFile);
                
                *lineNr = lineCnt;
                return retVal;
            }
            else
            {
                /* no need to re-initialize these, but in case if would be allowed
                 * for the keyregistry to silently skip erroneous lines, this would be needed
                 */
                *lineNr = 0;
                *errPos = 0;
                
                /* key and value is parsed in the line, add it to the list */
                storeKey(key, value);
            }
        }
        else
        {
            /* empty line, skip it */
        }
    }
    
    /* key and value has been stored, this memory can be released */
    free(line);
     
    return KREG_OK;
}

/**
 * @brief Retreives a key's value from the registry
 *
 * @param[in]  str input string containing the key
 * @param[out] key storage for parsed key from the input string
 * @param[out] value storage for parsed value from the input string
 * @param[out] character position where the parse failed
 * @return     KREG_OK
 *             KREG_KEY_INVALID
 *             KREG_KEY_EMPTY
 *             KREG_KEY_TOO_LONG
 */
uint8_t KREG_GetKey( char* str, char** key, char** value, uint16_t* errPos )
{
    uint8_t retVal;
    
    if ((retVal = parseKeyValue(key, value, str, strlen(str), errPos)) == KREG_OK)
    {        
        retVal = readKey(*key, value);
    }
    
    return retVal;
}

/**
 * @brief Saves a key-value pair in the registry
 *
 * @param[in]  str input string containing the key and value
 * @param[out] key storage for parsed key from the input string
 * @param[out] value storage for parsed value from the input string
 * @param[out] character position where the parse failed
 * @return     KREG_OK
 *             KREG_KEY_INVALID
 *             KREG_KEY_EMPTY
 *             KREG_KEY_TOO_LONG
 *             KREG_VAL_TOO_LONG
 */
uint8_t KREG_PutKey( char* str, char** key, char** value, uint16_t* errPos )
{
    uint8_t retVal;
    
    if ((retVal = parseKeyValue(key, value, str, strlen(str), errPos)) == KREG_OK)
    {        
        retVal = storeKey(*key, *value);
    }
    
    return retVal;
}