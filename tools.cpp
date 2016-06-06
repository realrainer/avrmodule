
#include "tools.h"

unsigned char const h264_start_code[4] = {0x00, 0x00, 0x00, 0x01};

int h264_getVopType( const void *p, int len ) {
    if ( !p || 6 >= len )
    return -1;

    unsigned char *b = (unsigned char*)p;

    // Verify NAL marker
    if ( b[ 0 ] || b[ 1 ] || 0x01 != b[ 2 ] ) {
        b++;
        if ( b[ 0 ] || b[ 1 ] || 0x01 != b[ 2 ] )
        return -1;
    }

    b += 3;
    // Verify VOP id
    if ( 0xb6 == *b ) {
        b++;
        return ( *b & 0xc0 ) >> 6;
    }

    switch( *b )
    {   case 0x65 : return 0;
        case 0x61 : return 1;
        case 0x01 : return 2;
    }
    return -1;
}
