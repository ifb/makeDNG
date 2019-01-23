makeDNG
===========

Simple tool for converting mosaiced (Bayer) TIFF files to Adobe DNG,
specifically suited for the Point Grey BFLY-U3-23S6C-C camera.

Usage:

    makeDNG input_tiff_file output_dng_file [cfa_pattern] [compression]
cfa_pattern can be from 0-3
  * 0 BGGR
  * 1 GBRG
  * 2 GRBG
  * 3 RGGB (default)

compression
  * 1 none (default)
  * 7 lossless JPEG
  * 8 Adobe Deflate (16-bit float)

Adobe Camera Raw sometimes decodes lossless JPEG files incorrectly, so this is
why lossless JPEG is not the default. Both RawTherapee 5.5 and ffmpeg decode
correctly, so I tend to think it is a bug in ACR. Use with caution if you care
about ACR.

The dcp subfolder contains the spectral data for the U3-23S6C as well as a script
to generate ForwardMatrix and ColorMatrix values with dcamprof.  The spectral
data is estimated from the graphic in the camera's data sheet, so don't put too
much trust in it.  A trivial patch is needed to add Ektaspace primaries to
dcamprof if that interests you.

Build instructions:

 * An MSVC project is included. I haven't bothered to test on non-Windows
   systems, but it should be fairly portable.
 * libtiff newer than around 4.0.6 is required for some of the DNG tags.
   Even when compression is used, no optional libs (zlib/zstd/lzma/libjpeg) are
   needed. We compress each tile with LJ92 ourselves and write them with
   TIFFWriteRawTile, but having libjpeg will suppress a warning from libtiff.
 * A patched libtiff is needed if you want to write some of the lens EXIF
   tags. This is very optional, but does avoid RawTherapee reporting your lens
   as "Unknown".

TODO:

 * Use threads to compress each tile
 * Add support for non-mod16 tile sizes (use padding)
 * Create proper CinemaDNG sequences
 * Implement the floating point X2 predictor (34894)

References:

Thanks to the following for their previous work:

prng by Ben Pfaff https://benpfaff.org/writings/clc/random.html  
elphel_dng by Dave Coffin http://community.elphel.com/files/jp4/elphel_dng.c  
makeDNG from the Field project http://openendedgroup.com/field/images/makeDNG.zip  
tiff2raw by Allan G. Weber http://sipi.usc.edu/database/tiff2raw.c  
rawtiDNG by Kevin Lawson https://github.com/kelvinlawson/rawti-tools  
Lossless JPEG by Andrew Baldwin https://bitbucket.org/baldand/mlrawviewer  
