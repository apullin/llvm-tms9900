/*
 * Minimal C library functions needed by FreeRTOS kernel.
 * These replace the standard libc for freestanding TMS9900 builds.
 */

typedef unsigned int size_t;

void * memset( void * s, int c, size_t n )
{
    unsigned char * p = ( unsigned char * ) s;

    while( n-- )
    {
        *p++ = ( unsigned char ) c;
    }

    return s;
}

void * memcpy( void * dest, const void * src, size_t n )
{
    unsigned char * d = ( unsigned char * ) dest;
    const unsigned char * s = ( const unsigned char * ) src;

    while( n-- )
    {
        *d++ = *s++;
    }

    return dest;
}

int memcmp( const void * s1, const void * s2, size_t n )
{
    const unsigned char * a = ( const unsigned char * ) s1;
    const unsigned char * b = ( const unsigned char * ) s2;

    while( n-- )
    {
        if( *a != *b )
        {
            return *a - *b;
        }

        a++;
        b++;
    }

    return 0;
}

size_t strlen( const char * s )
{
    const char * p = s;

    while( *p )
    {
        p++;
    }

    return ( size_t )( p - s );
}
