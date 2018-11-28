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

#define EXIFTAG_LENSMAKE 42035
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
    { EXIFTAG_LENSMAKE, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 1, "LensMake" },
    { TIFFTAG_FORWARDMATRIX1, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "ForwardMatrix1" },
    { TIFFTAG_FORWARDMATRIX2, -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, 1, 1, "ForwardMatrix2" }
};

static TIFFExtendProc parent_extender = NULL;  // In case we want a chain of extensions

static void registerCustomTIFFTags( TIFF *tif )
{
    /* Install the extended Tag field info */
    int error = TIFFMergeFieldInfo( tif, xtiffFieldInfo, sizeof( xtiffFieldInfo ) / sizeof( xtiffFieldInfo[0] ) );

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
    if( argc < 3 ) goto fail;

    static const float_t balance[] = { 1.0, 1.0, 1.0 }; // not using any camera gain
    static const uint16_t cfa_dimensions[] = { 2, 2 };
    static const float_t lensinfo[] = { 35.0f, 35.0f, 2.8f, 2.8f };
    static const double_t exposure_time[] = { 1.0f, 5.0f };
    static const double_t f_number = 4.0;
    static const uint16_t isospeed[] = { 100 };

    // I'm working with Ektachrome film, so dcamprof was patched to add Ektaspace primaries and then used to derive the matrices below
    // The spectral sensitivity chart in the Point Grey data sheet was used instead of actual ColorChecker test shots since that
    // seemed to produce better results. YMMV. You can always assign a .dcp file with RawTherapee later if you want to override this.

    // Ektaspace
    static const float_t cm1[] = { 1.299046f, -0.514857f, -0.123131f, -0.130278f, 1.028754f,  0.117381f, -0.053247f,  0.190644f, 0.633399f };
    static const float_t fm1[] = { 0.516209f,  0.387509f,  0.060500f,  0.059270f, 1.054966f, -0.114236f,  0.028743f, -0.288736f, 1.085194f };

    // Ektaspace dual (StdA, D65)
    // static const float_t cm1[] = { 1.653300f, -0.863900f, -0.100000f, -0.038600f, 0.902700f,  0.163000f, -0.001600f,  0.122400f, 0.679300f };
    // static const float_t cm2[] = { 1.197700f, -0.432800f, -0.127300f, -0.150800f, 1.051500f,  0.113800f, -0.066300f,  0.213800f, 0.621800f };
    // static const float_t fm1[] = { 0.533600f,  0.324300f,  0.106400f, -0.022900f, 1.072500f, -0.049500f,  0.020400f, -0.390900f, 1.195500f };
    // static const float_t fm2[] = { 0.513200f,  0.420400f,  0.030700f,  0.084300f, 1.062400f, -0.146700f,  0.025300f, -0.244200f, 1.044100f };

    // sRGB
    // static const float_t cm1[] = { 1.262754f, -0.489137f, -0.111894f, -0.172647f, 1.051837f,  0.138915f, -0.130211f,  0.297795f, 0.593481f };
    // static const float_t fm1[] = { 0.534702f,  0.395518f,  0.033998f,  0.067625f, 1.064278f, -0.131903f,  0.061814f, -0.420885f, 1.184272f };

    // Munsell colors
    // static const float_t cm1[] = { 1.371415f, -0.637680f, -0.058966f, -0.113604f, 0.962723f,  0.177957f, -0.083913f,  0.211040f, 0.644405f };
    // static const float_t fm1[] = { 0.511201f,  0.426254f,  0.026764f,  0.062997f, 1.064999f, -0.127995f,  0.057267f, -0.362485f, 1.130420f };

    uint32_t width = 0, height = 0, bpp = 0, spp = 0, rps = 0;
    uint64_t exif_dir_offset = 0;

    int cfa = CFA_RGGB;
    if( argc > 3 ) // runtime-specified CFA pattern (useful if the image is flipped/rotated)
        cfa = atoi( argv[3] );

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

    if( !(tif_in = TIFFOpen( argv[1], "r" )) )
    {
        perror( argv[1] );
        return 1;
    }

    if( !(tif = TIFFOpen( argv[2], "w" )) )
        goto fail;

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

    TIFFSetField( tif, TIFFTAG_DNGVERSION, "\001\002\00\00" );
    TIFFSetField( tif, TIFFTAG_DNGBACKWARDVERSION, "\001\0\0\0" );
    TIFFSetField( tif, TIFFTAG_SUBFILETYPE, 0 );
    TIFFSetField( tif, TIFFTAG_IMAGEWIDTH, width );
    TIFFSetField( tif, TIFFTAG_IMAGELENGTH, height );
    TIFFSetField( tif, TIFFTAG_BITSPERSAMPLE, bpp );
    TIFFSetField( tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE );
    TIFFSetField( tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA );
    TIFFSetField( tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB );
    TIFFSetField( tif, TIFFTAG_MAKE, "Point Grey" );
    TIFFSetField( tif, TIFFTAG_MODEL, "BF-U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );
    TIFFSetField( tif, TIFFTAG_SAMPLESPERPIXEL, spp );
    TIFFSetField( tif, TIFFTAG_ROWSPERSTRIP, rps );
    TIFFSetField( tif, TIFFTAG_XRESOLUTION, roundf( 7250 ) );
    TIFFSetField( tif, TIFFTAG_YRESOLUTION, roundf( 7250 ) );
    TIFFSetField( tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH );
    TIFFSetField( tif, TIFFTAG_SOFTWARE, "makeDNG 0.1" );
    TIFFSetField( tif, TIFFTAG_DATETIME, datetime );
    TIFFSetField( tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT );
    TIFFSetField( tif, TIFFTAG_CFAREPEATPATTERNDIM, cfa_dimensions );
    TIFFSetField( tif, TIFFTAG_CFAPATTERN, cfa_patterns[cfa] );
    TIFFSetField( tif, TIFFTAG_UNIQUECAMERAMODEL, "Point Grey Blackfly U3-23S6C-C" );
    TIFFSetField( tif, TIFFTAG_CFAPLANECOLOR, 3, "\00\01\02" ); // RGB
    TIFFSetField( tif, TIFFTAG_CFALAYOUT, 1 ); // rectangular or square (not staggered)
    TIFFSetField( tif, TIFFTAG_COLORMATRIX1, 9, cm1 );
    // TIFFSetField( tif, TIFFTAG_COLORMATRIX2, 9, cm2 );
    TIFFSetField( tif, TIFFTAG_ANALOGBALANCE, 3, balance );
    TIFFSetField( tif, TIFFTAG_CAMERASERIALNUMBER, "15187959" );
    TIFFSetField( tif, TIFFTAG_LENSINFO, lensinfo );
    TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT1, 23 ); // StdA=17, D65=21, D50=23
    // TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT1, 17 );
    // TIFFSetField( tif, TIFFTAG_CALIBRATIONILLUMINANT2, 21 );
    TIFFSetField( tif, TIFFTAG_RAWDATAUNIQUEID, uuid );
    TIFFSetField( tif, TIFFTAG_FORWARDMATRIX1, 9, fm1 );
    // TIFFSetField( tif, TIFFTAG_FORWARDMATRIX2, 9, fm2 );

    uint32_t* buf = 0;
    buf = _TIFFmalloc( TIFFScanlineSize( tif_in ) );
    for( uint32_t row = 0; row < height; row++ )
    {
        TIFFReadScanline( tif_in, buf, row, 0 );
        TIFFWriteScanline( tif, buf, row, 0 );
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
    TIFFSetField( tif, EXIFTAG_IMAGEUNIQUEID, uuid_str );
    TIFFSetField( tif, EXIFTAG_LENSMAKE, "Canon" );
    TIFFWriteCustomDirectory( tif, &exif_dir_offset );
    TIFFSetDirectory( tif, 0 );
    TIFFSetField( tif, TIFFTAG_EXIFIFD, exif_dir_offset );

    _TIFFfree( buf );
    TIFFClose( tif_in );
    TIFFClose( tif );
    status = 0;
    return status;
fail:
    printf( "usage: makeDNG input_tiff_file output_dng_file [cfa_pattern]\n\n" );
    printf( "       cfa_pattern 0: BGGR\n" );
    printf( "                   1: GBRG\n" );
    printf( "                   2: GRBG\n" );
    printf( "                   3: RGGB (default)\n" );
    return status;
}
