/*****************************************************************************/
// Copyright 2006-2012 Adobe Systems Incorporated
// All Rights Reserved.
//
// NOTICE:  Adobe permits you to use, modify, and distribute this file in
// accordance with the terms of the Adobe license agreement accompanying it.
/*****************************************************************************/

#include "dng_utils.h"

uint16_t DNG_FloatToHalf( uint32_t i )
{
    int32_t sign = (i >> 16) & 0x00008000;
    int32_t exponent = ((i >> 23) & 0x000000ff) - (127 - 15);
    int32_t mantissa = i & 0x007fffff;
    if( exponent <= 0 )
    {
        if( exponent < -10 )
        {
            return (uint16_t)sign;
        }
        mantissa = (mantissa | 0x00800000) >> (1 - exponent);
        if( mantissa & 0x00001000 )
            mantissa += 0x00002000;
        return (uint16_t)(sign | (mantissa >> 13));
    }
    else if( exponent == 0xff - (127 - 15) )
    {
        if( mantissa == 0 )
        {
            return (uint16_t)(sign | 0x7c00);
        }
        else
        {
            return (uint16_t)(sign | 0x7c00 | (mantissa >> 13));
        }
    }
    if( mantissa & 0x00001000 )
    {
        mantissa += 0x00002000;
        if( mantissa & 0x00800000 )
        {
            mantissa = 0;     // overflow in significand,
            exponent += 1;    // adjust exponent
        }
    }
    if( exponent > 30 )
    {
        return (uint16_t)(sign | 0x7c00); // infinity with the same sign as f.
    }
    return (uint16_t)(sign | (exponent << 10) | (mantissa >> 13));
}

uint32_t float_bits( const float f )
{
    union
    {
        uint32_t u;
        float f;
    } temp;

    temp.f = f;
    return temp.u;
}
