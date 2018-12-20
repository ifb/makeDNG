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

#define TIFFTAG_FORWARDMATRIX1 50964
#define TIFFTAG_FORWARDMATRIX2 50965

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
    { TIFFTAG_FORWARDMATRIX2, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "ForwardMatrix2" }
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
    static const float_t balance_D50[]   = { 1.57f, 1.00f, 1.51f };
    static const float_t balance_D55[]   = { 1.67f, 1.00f, 1.40f };
    static const float_t balance_D65[]   = { 1.82f, 1.00f, 1.25f };
    static const float_t balance_StdA[]  = { 1.00f, 1.00f, 2.53f };

    static const float_t as_shot_d50[] = { 0.636099f, 1.0f ,0.661984f };

    static const uint16_t cfa_dimensions[] = { 2, 2 };
    static const double_t exposure_time[] = { 1.0f, 5.0f };
    static const double_t f_number = 4.0;
    static const uint16_t isospeed[] = { 100 };
    static const float_t *balance = balance_D50;

    // I'm working with Ektachrome film, so dcamprof was patched to add Ektaspace primaries and then used to derive the matrices below.
    // The spectral sensitivity chart in the Point Grey data sheet was used instead of actual ColorChecker test shots since that
    // seemed to produce better results. YMMV. You can always assign a .dcp file with RawTherapee later if you want to override this.

    static const float_t cm1[] = { 1.299046f, -0.514857f, -0.123131f, -0.130278f, 1.028754f,  0.117381f, -0.053247f,  0.190644f, 0.633399f };
    static const float_t fm1[] = { 0.516209f,  0.387509f,  0.060500f,  0.059270f, 1.054966f, -0.114236f,  0.028743f, -0.288736f, 1.085194f };
    static const int illuminant = 23; // StdA=17, D50=23, D55=20, D65=21

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
    if( compression != COMPRESSION_NONE && compression != COMPRESSION_JPEG )
        goto usage;

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
    TIFFSetField( tif, TIFFTAG_DNGVERSION, "\01\02\00\00" );
    TIFFSetField( tif, TIFFTAG_DNGBACKWARDVERSION, "\01\02\00\00" );
    TIFFSetField( tif, TIFFTAG_SUBFILETYPE, 0 );
    TIFFSetField( tif, TIFFTAG_IMAGEWIDTH, width );
    TIFFSetField( tif, TIFFTAG_IMAGELENGTH, height );
    TIFFSetField( tif, TIFFTAG_BITSPERSAMPLE, bpp );
    TIFFSetField( tif, TIFFTAG_COMPRESSION, compression );
    TIFFSetField( tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA );
    TIFFSetField( tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB );
    TIFFSetField( tif, TIFFTAG_MAKE, "Canon" ); // hack to enable LJ92 mode in RawTherapee
    TIFFSetField( tif, TIFFTAG_MODEL, "BF-U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );
    TIFFSetField( tif, TIFFTAG_SAMPLESPERPIXEL, spp );
    TIFFSetField( tif, TIFFTAG_XRESOLUTION, roundf( 7250 ) );
    TIFFSetField( tif, TIFFTAG_YRESOLUTION, roundf( 7250 ) );
    TIFFSetField( tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH );
    TIFFSetField( tif, TIFFTAG_SOFTWARE, "makeDNG 0.2" );
    TIFFSetField( tif, TIFFTAG_DATETIME, datetime );
    if( compression == COMPRESSION_JPEG )
    {
        TIFFSetField( tif, TIFFTAG_TILEWIDTH, halfwidth );
        TIFFSetField( tif, TIFFTAG_TILELENGTH, height );
    }
    else
        TIFFSetField( tif, TIFFTAG_ROWSPERSTRIP, rps );
    TIFFSetField( tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT );
    TIFFSetField( tif, TIFFTAG_CFAREPEATPATTERNDIM, cfa_dimensions );
    TIFFSetField( tif, TIFFTAG_CFAPATTERN, cfa_patterns[cfa] );
    TIFFSetField( tif, TIFFTAG_UNIQUECAMERAMODEL, "Point Grey Blackfly U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_CFAPLANECOLOR, 3, "\00\01\02" ); // RGB
    TIFFSetField( tif, TIFFTAG_CFALAYOUT, 1 ); // rectangular or square (not staggered)
    TIFFSetField( tif, TIFFTAG_COLORMATRIX1, 9, cm1 );
    // TIFFSetField( tif, TIFFTAG_COLORMATRIX2, 9, cm2 );
    TIFFSetField( tif, TIFFTAG_ANALOGBALANCE, 3, balance );
    TIFFSetField( tif, TIFFTAG_ASSHOTNEUTRAL, 3, as_shot_d50 );
    TIFFSetField( tif, TIFFTAG_CAMERASERIALNUMBER, "15187959" );
    TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT1, illuminant );
    // TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT1, 17 );
    // TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT2, 21 );
    TIFFSetField( tif, TIFFTAG_RAWDATAUNIQUEID, uuid );
    TIFFSetField( tif, TIFFTAG_FORWARDMATRIX1, 9, fm1 );
    // TIFFSetField( tif, TIFFTAG_FORWARDMATRIX2, 9, fm2 );

    uint8_t* buf = 0;
    buf = _TIFFmalloc( TIFFScanlineSize( tif_in ) * height );
    uint8_t* position = buf;
    for( unsigned int row = 0; row < height; row++ )
    {
        TIFFReadScanline( tif_in, position, row, 0 );
        if( compression == COMPRESSION_NONE )
            TIFFWriteScanline( tif, position, row, 0 );
        position += width * 2;
    }

    if( compression == COMPRESSION_JPEG )
    {
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
    TIFFSetField( tif, EXIFTAG_FOCALLENGTH, 81.0f );
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
    TIFFSetField( tif, EXIFTAG_LENSMAKE, "Canon" );
    TIFFSetField( tif, EXIFTAG_LENSMODEL, "Macrophoto 35mm f/2.8" );
    TIFFSetField( tif, EXIFTAG_LENSSERIALNUMBER, "13718" );
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
    printf( "usage: makeDNG input_tiff_file output_dng_file [cfa_pattern] [compression]\n\n" );
    printf( "       cfa_pattern 0: BGGR\n" );
    printf( "                   1: GBRG\n" );
    printf( "                   2: GRBG\n" );
    printf( "                   3: RGGB (default)\n\n" );
    printf( "       compression 1: none (default)\n" );
    printf( "                   7: lossless JPEG\n" );
    return status;
fail:
    return status;
}
