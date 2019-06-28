/*********************************************************************
 *  Projet "Systèmes d'Exploitation"
 *  Licence Informatique - Université de Cergy
 * 
 *  Auteur: Maaloul Amira 40%, El Mouaouine Mehdi 40%, El Baraka Amine 20%
 *  Testeurs: El Baraka Amine
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fs.h"



/**
 * This function creates random number of chars for string
 *
 * @param str The string to manipulate
 * @param size The size of string
 *
 * @return The size of newly generated string
 */
int rand_string( char * str, size_t size)
{
    if(size < 1)
        return 0;
    
    int n, key;
    
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    for (n = 0; n < size; n++)
    {
        key = rand() % (int) (sizeof charset - 1);
        str[n] = charset[key];
    }
    str[size] = '\0';
    
    return size + 1;
}



/**
 * This function toggles a bit in the given array at the given index
 *
 * @param array The array of char
 * @param index Position of the bit to toggle
 *
 */
void toggle_bit( char * array, int index )
{
    array[index/8] ^= 1 << ( index % 8 );
}



/**
 * This function returns the value at a given index in the given array
 *
 * @param  array 
 * @param index 
 *
 * @returns char
 */
char get_bit( char * array, int index )
{
    return 1 & ( array[index/8] >> ( index % 8 ));
}

/**
 * This function set a value in the given array at the given index
 *
 * @param array The array of char
 * @param index The index 
 * @param value The value to set
 *
 */
void set_bit( char * array, int index, char value )
{
    if( value != 0 && value != 1 )
    {
        return;
    }
    array[index/8] ^= 1 << ( index % 8 );
    
    if( get_bit( array, index ) == value)
    {
        return;
    }
    
    toggle_bit( array, index );
}



/**
 * This function checks for a free inode and then returns it if there is one
 *
 * @return The free inode number
 *
 */
int get_free_inode()
{
    int i = 0;
    
    for( i = 0; i < MAX_INODE; i++ )
    {
        if( get_bit( inodeMap, i ) == 0)
        {            
            set_bit( inodeMap, i, 1 );
            
            superBlock.freeInodeCount--;
            
            return i;
        }
    }
    
    return -1;
}



/**
 * This function returns the block number of a free block if there is one
 * 
 * @return The block number or -1 if there is no free blocks
 *
 */
int get_free_block()
{
    int i = 0;
    
    for( i = 0; i < MAX_BLOCK; i++ )
    {
        if( get_bit( blockMap, i ) == 0 )
        {
            set_bit( blockMap, i, 1 );
            superBlock.freeBlockCount--;
            
            return i;
        }
    }
    
    return -1;
}



/**
 * This function sets / formats the correct time by retrieving the system time
 *
 * @param tv
 * @param buf
 * @param sz
 *
 * @return The size of newly generated string
 */
int format_timeval( struct timeval * tv, char * buf, size_t sz )
{
    ssize_t written = -1;
    struct tm * gm;
    gm = gmtime( &tv -> tv_sec );
    
    if( gm )
    {
        written = ( ssize_t )strftime( buf, sz, "%Y-%m-%d %H:%M:%S", gm );
        
        if(( written > 0 ) && (( size_t )written < sz ))
        {
            int w = snprintf( buf + written, sz - ( size_t )written, ".%06dZ", tv -> tv_usec );
            written = (w > 0) ? written + w : -1;
        }
    }
    
    return written;
}

/**
 * This function is used to compare two strings
 * 
 * @param str1 The first string to compare
 * @param str2 The second string to compare
 * 
 * @return 1 if the strings are equal, 0 otherwise
 * 
 */
int str_equal(char *str1, char *str2)
{
    if (strlen(str1) == strlen(str2) && strncmp(str1, str2, strlen(str1)) == 0)
    {
        return 1;
    }

    return 0;
}
