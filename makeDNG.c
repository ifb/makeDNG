/*****************************************************************************
 * makeDNG: a utility for converting mosaiced TIFF files to DNG
 *****************************************************************************
 * Copyright (C) 2018 Phillip Blucas
 *
 * See other related projects for more information:
 *
 * elphel_dng by Dave Coffin:
 *     http://community.elphel.com/files/jp4/elphel_dng.c
 *     https://www3.elphel.com/importwiki?title=JP4
 * makeDNG from the Field project:
 *     http://openendedgroup.com/field/images/makeDNG.zip
 * tiff2raw by Allan G. Weber:
 *     http://sipi.usc.edu/database/tiff2raw.c
 * rawtiDNG by Kevin Lawson:
 *     https://github.com/kelvinlawson/rawti-tools
 * Lossless JPEG (T.81 Annex H) by Andrew Baldwin:
 *     https://bitbucket.org/baldand/mlrawviewer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 *****************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <tiffio.h>

#include "prng.h"
#include "lj92.h"
#include "dng_utils.h"

#define TIFFTAG_FORWARDMATRIX1 50964
#define TIFFTAG_FORWARDMATRIX2 50965
#define TIFFTAG_TIMECODES 51043
#define TIFFTAG_FRAMERATE 51044
#define TIFFTAG_REELNAME 51081

enum tiff_cfa_color
{
    CFA_RED = 0,
    CFA_GREEN = 1,
    CFA_BLUE = 2,
};

enum cfa_pattern
{
    CFA_BGGR = 0,
    CFA_GBRG,
    CFA_GRBG,
    CFA_RGGB,
    CFA_NUM_PATTERNS,
};

static const char cfa_patterns[4][CFA_NUM_PATTERNS] = {
    [CFA_BGGR] = { CFA_BLUE, CFA_GREEN, CFA_GREEN, CFA_RED },
    [CFA_GBRG] = { CFA_GREEN, CFA_BLUE, CFA_RED, CFA_GREEN },
    [CFA_GRBG] = { CFA_GREEN, CFA_RED, CFA_BLUE, CFA_GREEN },
    [CFA_RGGB] = { CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE },
};

static const TIFFFieldInfo xtiffFieldInfo[] = {
    { TIFFTAG_FORWARDMATRIX1, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "ForwardMatrix1" },
    { TIFFTAG_FORWARDMATRIX2, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "ForwardMatrix2" },
    { TIFFTAG_TIMECODES, -1, -1, TIFF_BYTE, FIELD_CUSTOM, 1, 1, "TimeCodes" },
    { TIFFTAG_FRAMERATE, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "FrameRate" },
    { TIFFTAG_REELNAME, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0, "ReelName" }
};

static TIFFExtendProc parent_extender = NULL;  // In case we want a chain of extensions

static void registerCustomTIFFTags( TIFF *tif )
{
    // Install the extended Tag field info
    TIFFMergeFieldInfo( tif, xtiffFieldInfo, sizeof( xtiffFieldInfo ) / sizeof( xtiffFieldInfo[0] ) );

    if( parent_extender )
        (*parent_extender)(tif);
}

static void augment_libtiff_with_custom_tags( void )
{
    static int first_time = 1;
    if( !first_time )
        return;
    first_time = 0;
    parent_extender = TIFFSetTagExtender( registerCustomTIFFTags );
}

int main( int argc, char **argv )
{
    int status = 1;
    if( argc < 3 ) goto usage;

    // White balance gains calculated with dcamprof
    static const float_t balance_unity[] = { 1.00f, 1.00f, 1.00f };
    static const float_t balance_D50[] = { 1.57f, 1.00f, 1.51f };
    static const float_t balance_D55[] = { 1.67f, 1.00f, 1.40f };
    static const float_t balance_D65[] = { 1.82f, 1.00f, 1.25f };
    static const float_t balance_D75[] = { 1.93f, 1.00f, 1.15f };
    static const float_t balance_StdA[] = { 1.00f, 1.00f, 2.53f };

    static const float_t as_shot_D50[] = { 0.636099f, 1.0f, 0.661984f };
    static const float_t as_shot_D55[] = { 0.599260f, 1.0f, 0.713991f };
    static const float_t as_shot_D65[] = { 0.549323f, 1.0f, 0.802144f };
    static const float_t as_shot_D75[] = { 0.518043f, 1.0f, 0.872091f };
    static const float_t as_shot_StdA[] = { 0.998233f, 1.0f, 0.394600f };

    static const uint16_t cfa_dimensions[] = { 2, 2 };
    static const double_t exposure_time[] = { 1.0f, 5.0f };
    static const double_t f_number = 2.5f;
    static const uint16_t isospeed[] = { 90 };
    static const float_t *balance = balance_unity;
    static const float_t *as_shot = as_shot_D55;
    static const float_t resolution = 7300.0f;
    static const float_t framerate[] = { 18, 1 };
    uint8_t timecode[8] = { 0 };

    // I'm working with Ektachrome film, so dcamprof was patched to add Ektaspace primaries and then used to derive the matrices below.
    // The spectral sensitivity chart in the Point Grey data sheet was used instead of actual ColorChecker test shots since that
    // seemed to produce better results. YMMV. You can always assign a .dcp file with RawTherapee later if you want to override this.

    static const float_t cm1[] = { 1.299046f, -0.514857f, -0.123131f, -0.130278f, 1.028754f,  0.117381f, -0.053247f,  0.190644f, 0.633399f };
    static const float_t fm1[] = { 0.516209f,  0.387509f,  0.060500f,  0.059270f, 1.054966f, -0.114236f,  0.028743f, -0.288736f, 1.085194f };
    static const int illuminant1 = 23; // StdA=17, D50=23, D55=20, D65=21

    uint32_t width = 0, height = 0, bpp = 0, spp = 0, rps = 0;
    uint64_t exif_dir_offset = 0;

    int cfa = CFA_RGGB;
    if( argc > 3 ) // runtime-specified CFA pattern (useful if the image is flipped/rotated)
        cfa = atoi( argv[3] );
    if( cfa > 3 || cfa < 0 )
        goto usage;

    int compression = COMPRESSION_NONE;
    if( argc > 4 )
        compression = atoi( argv[4] );
    if( compression != COMPRESSION_NONE && compression != COMPRESSION_JPEG &&
        compression != COMPRESSION_ADOBE_DEFLATE )
        goto usage;

    int frame = 0;
    if( argc > 6 )
        frame = atoi( argv[6] );
    if( frame < 0 )
        goto usage;

    if( frame )
    {
        // Time code is an integer cast to a hex string for our purposes
        // There's more to it in SMPTE 12M/309/331 if you want to get into drop-frame or date/time
        // For example, to indicate 17 frames you write 0x17 (not 0x11)
        char buf[5];
        timecode[3] = (int)( frame / ( 3600 * framerate[0]/framerate[1] ) );
        sprintf( buf, "0x%d", timecode[3] );
        timecode[3] = (int)strtol( buf, NULL, 16 );
        timecode[2] = (int)( frame / (   60 * framerate[0]/framerate[1] ) ) % 60;
        sprintf( buf, "0x%d", timecode[2] );
        timecode[2] = (int)strtol( buf, NULL, 16 );
        timecode[1] = (int)( frame / (        framerate[0]/framerate[1] ) ) % 60;
        sprintf( buf, "0x%d", timecode[1] );
        timecode[1] = (int)strtol( buf, NULL, 16 );
        timecode[0] = (int)fmod( frame, framerate[0]/framerate[1] );
        sprintf( buf, "0x%d", timecode[0] );
        timecode[0] = (int)strtol( buf, NULL, 16 );
    }

    const uint8_t version4[] = "\01\04\00\00";
    const uint8_t version2[] = "\01\02\00\00";
    const uint8_t* version = version2;
    int sampleformat = SAMPLEFORMAT_UINT;
    if( compression == COMPRESSION_ADOBE_DEFLATE )
    {
        version = version4;
        sampleformat = SAMPLEFORMAT_IEEEFP;
    }

    uint8_t uuid[16] = { 0 };
    char uuid_str[33] = { 0 };
    prng_get_bytes( uuid, sizeof( uuid ) );
    uuid[6] &= 0x0F;
    uuid[6] |= ((4 << 4) & 0xF0); // version 4
    uuid[8] &= 0x3F;
    uuid[8] |= ((2 << 6) & 0xC0); // variant 2
    sprintf( uuid_str, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15] );

    augment_libtiff_with_custom_tags();
    TIFF *tif = 0, *tif_in = 0;

    if( (tif_in = TIFFOpen( argv[1], "r" )) == NULL )
    {
        perror( argv[1] );
        goto fail;
    }

    if( (tif = TIFFOpen( argv[2], "w" )) == NULL )
    {
        perror( argv[1] );
        goto fail;
    }

    TIFFGetField( tif_in, TIFFTAG_IMAGEWIDTH, &width );
    TIFFGetField( tif_in, TIFFTAG_IMAGELENGTH, &height );
    TIFFGetField( tif_in, TIFFTAG_BITSPERSAMPLE, &bpp );
    TIFFGetField( tif_in, TIFFTAG_SAMPLESPERPIXEL, &spp );
    TIFFGetField( tif_in, TIFFTAG_ROWSPERSTRIP, &rps );

    struct stat st = { 0 };
    struct tm *tm = { 0 };
    char datetime[20] = { 0 };
    stat( argv[1], &st );
    tm = gmtime( &st.st_mtime );
    snprintf( datetime, sizeof( datetime ), "%04d:%02d:%02d %02d:%02d:%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec );

    const uint32_t halfwidth = width / 2;
    if( halfwidth % 16 || height % 16 )
    {
        fprintf( stderr, "Tile dimensions must be a multiple of 16.\n" );
        goto fail;
    }

    TIFFSetField( tif, TIFFTAG_DNGVERSION, version );
    TIFFSetField( tif, TIFFTAG_DNGBACKWARDVERSION, version );
    TIFFSetField( tif, TIFFTAG_SUBFILETYPE, 0 );
    TIFFSetField( tif, TIFFTAG_IMAGEWIDTH, width );
    TIFFSetField( tif, TIFFTAG_IMAGELENGTH, height );
    TIFFSetField( tif, TIFFTAG_BITSPERSAMPLE, bpp );
    TIFFSetField( tif, TIFFTAG_COMPRESSION, compression );
    TIFFSetField( tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA );
    TIFFSetField( tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB );
    TIFFSetField( tif, TIFFTAG_MAKE, compression == COMPRESSION_JPEG ? "Canon" : "Point Grey" ); // hack to enable LJ92 mode in RawTherapee
    TIFFSetField( tif, TIFFTAG_MODEL, "BFLY-U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );
    TIFFSetField( tif, TIFFTAG_SAMPLESPERPIXEL, spp );
    TIFFSetField( tif, TIFFTAG_XRESOLUTION, resolution );
    TIFFSetField( tif, TIFFTAG_YRESOLUTION, resolution );
    TIFFSetField( tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH );
    TIFFSetField( tif, TIFFTAG_SOFTWARE, "makeDNG 0.3" );
    TIFFSetField( tif, TIFFTAG_DATETIME, datetime );
    TIFFSetField( tif, TIFFTAG_SAMPLEFORMAT, sampleformat );
    TIFFSetField( tif, TIFFTAG_CFAREPEATPATTERNDIM, cfa_dimensions );
    TIFFSetField( tif, TIFFTAG_CFAPATTERN, cfa_patterns[cfa] );
    TIFFSetField( tif, TIFFTAG_UNIQUECAMERAMODEL, "Point Grey Blackfly U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_CFAPLANECOLOR, 3, "\00\01\02" ); // RGB
    TIFFSetField( tif, TIFFTAG_CFALAYOUT, 1 ); // rectangular or square (not staggered)
    TIFFSetField( tif, TIFFTAG_COLORMATRIX1, 9, cm1 );
    // TIFFSetField( tif, TIFFTAG_COLORMATRIX2, 9, cm2 );
    TIFFSetField( tif, TIFFTAG_ANALOGBALANCE, 3, balance );
    TIFFSetField( tif, TIFFTAG_ASSHOTNEUTRAL, 3, as_shot );
    TIFFSetField( tif, TIFFTAG_CAMERASERIALNUMBER, "15187959" );
    TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT1, illuminant1 );
    // TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT2, illuminant2 );
    TIFFSetField( tif, TIFFTAG_RAWDATAUNIQUEID, uuid );
    TIFFSetField( tif, TIFFTAG_FORWARDMATRIX1, 9, fm1 );
    // TIFFSetField( tif, TIFFTAG_FORWARDMATRIX2, 9, fm2 );
    if( frame )
    {
        TIFFSetField( tif, TIFFTAG_TIMECODES, 8, timecode );
        TIFFSetField( tif, TIFFTAG_FRAMERATE, 2, framerate );
    }
    if( argc > 5 )
        TIFFSetField( tif, TIFFTAG_REELNAME, argv[5] );

    uint8_t* buf = 0;
    buf = _TIFFmalloc( TIFFScanlineSize( tif_in ) * height );

    for( uint32_t row = 0; row < height; row++ )
        TIFFReadScanline( tif_in, &buf[row * width * 2], row, 0 );

    if( compression == COMPRESSION_NONE )
    {
        TIFFSetField( tif, TIFFTAG_ROWSPERSTRIP, rps );
        for( uint32_t row = 0; row < height; row++ )
            TIFFWriteScanline( tif, &buf[row * width * 2], row, 0 );
    }
    else if( compression == COMPRESSION_ADOBE_DEFLATE )
    {
        TIFFSetField( tif, TIFFTAG_TILEWIDTH, halfwidth );
        TIFFSetField( tif, TIFFTAG_TILELENGTH, height );
        TIFFSetField( tif, TIFFTAG_ZIPQUALITY, 9 );
        TIFFSetField( tif, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT );
        uint16_t* buf1 = _TIFFmalloc( TIFFScanlineSize( tif_in ) * height / 2 );
        uint16_t* buf2 = _TIFFmalloc( TIFFScanlineSize( tif_in ) * height / 2 );
        const float_t scale = 1.0f / 65535.0f;
        uint16_t* buf16 = (uint16_t*)buf;
        for( uint32_t row = 0; row < height; row++ )
        {
            for( uint32_t i = 0; i < halfwidth; i++ )
                buf1[row * halfwidth + i] = DNG_FloatToHalf( float_bits( buf16[row * width + i] * scale ) );
            for( uint32_t i = halfwidth; i < width; i++ )
                buf2[row * halfwidth + i - halfwidth] = DNG_FloatToHalf( float_bits( buf16[row * width + i] * scale ) );
        }
        TIFFWriteTile( tif, buf1, 0, 0, 0, sizeof( buf1 ) );
        TIFFWriteTile( tif, buf2, halfwidth, 0, 0, sizeof( buf2 ) );
        _TIFFfree( buf1 );
        _TIFFfree( buf2 );
    }
    else if( compression == COMPRESSION_JPEG )
    {
        TIFFSetField( tif, TIFFTAG_TILEWIDTH, halfwidth );
        TIFFSetField( tif, TIFFTAG_TILELENGTH, height );
        int ret = 0;
        uint8_t* input = buf;
        uint8_t* encoded = NULL;
        int encodedLength = 0;
        ret = lj92_encode( (uint16_t*)&input[0], halfwidth, height, 16, halfwidth, halfwidth, NULL, 0, &encoded, &encodedLength );
        TIFFWriteRawTile( tif, 0, encoded, encodedLength );
        free( encoded );
        ret = lj92_encode( (uint16_t*)&input[width], halfwidth, height, 16, halfwidth, halfwidth, NULL, 0, &encoded, &encodedLength );
        TIFFWriteRawTile( tif, 1, encoded, encodedLength );
        free( encoded );
    }

    TIFFWriteDirectory( tif );
    TIFFCreateEXIFDirectory( tif );
    TIFFSetField( tif, EXIFTAG_FOCALLENGTH, 107.0f );
    TIFFSetField( tif, EXIFTAG_EXPOSURETIME, exposure_time[0] / exposure_time[1] );
    TIFFSetField( tif, EXIFTAG_FNUMBER, f_number );
    TIFFSetField( tif, EXIFTAG_ISOSPEEDRATINGS, 1, isospeed );
    TIFFSetField( tif, EXIFTAG_EXPOSUREPROGRAM, 1 ); // manual
    TIFFSetField( tif, EXIFTAG_DATETIMEORIGINAL, datetime );
    TIFFSetField( tif, EXIFTAG_DATETIMEDIGITIZED, datetime );
    TIFFSetField( tif, EXIFTAG_SHUTTERSPEEDVALUE, log2( exposure_time[0] / exposure_time[1] ) * -1 );
    TIFFSetField( tif, EXIFTAG_APERTUREVALUE, log2( f_number * f_number ) );
    TIFFSetField( tif, EXIFTAG_FLASH, 32 ); // no flash function
    TIFFSetField( tif, EXIFTAG_SENSINGMETHOD, 2 );
    TIFFSetField( tif, EXIFTAG_IMAGEUNIQUEID, uuid_str );
#ifdef HAVE_CUSTOM_EXIFTAGS
    TIFFSetField( tif, EXIFTAG_TIFFEPSTANDARDID, "\01\00\00\00" );
    TIFFSetField( tif, EXIFTAG_LENSMAKE, "Minolta" );
    TIFFSetField( tif, EXIFTAG_LENSMODEL, "M5400 36mm f/2.5" );
    TIFFSetField( tif, EXIFTAG_LENSSERIALNUMBER, "20401326" );
#endif
    TIFFWriteCustomDirectory( tif, &exif_dir_offset );
    TIFFSetDirectory( tif, 0 );
    TIFFSetField( tif, TIFFTAG_EXIFIFD, exif_dir_offset );

    _TIFFfree( buf );
    TIFFClose( tif_in );
    TIFFClose( tif );
    status = 0;
    return status;
usage:
    printf( "usage: makeDNG input_tiff_file output_dng_file [cfa_pattern] [compression]\n" );
    printf( "               [reelname] [frame number]\n\n" );
    printf( "       cfa_pattern 0: BGGR\n" );
    printf( "                   1: GBRG\n" );
    printf( "                   2: GRBG\n" );
    printf( "                   3: RGGB (default)\n\n" );
    printf( "       compression 1: none (default)\n" );
    printf( "                   7: lossless JPEG\n" );
    printf( "                   8: Adobe Deflate (16-bit float)\n" );
    return status;
fail:
    return status;
}
