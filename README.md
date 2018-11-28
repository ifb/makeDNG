makeDNG
===========

Simple tool for converting mosaiced TIFF files to Adobe DNG, specifically suited
for the Point Grey BFLY-U3-23S6C-C.

Usage:

    makeDNG input_tiff_file output_dng_file [cfa_pattern]
cfa_pattern can be from 0-3
  * 0 BGGR
  * 1 GBRG
  * 2 GRBG
  * 3 RGGB (default)

Build instructions:

 * An MSVC project is included. I haven't bothered to test on non-Windows
   systems, but it should be fairly portable.
 * libtiff newer than around 4.0.6 is required for some of the DNG tags.
   Compression is not used, so no optional libs (zlib/zstd/lzma/libjpeg) are needed.

The dcp subfolder contains the spectral data for the U3-23S6C as well as a script
to generate ForwardMatrix and ColorMatrix values with dcamprof.  The spectral
data is estimated from the graphic in the camera's data sheet, so don't put too
much trust in it.  A trivial patch is needed to add Ektaspace primaries to
dcamprof if that interests you.

TODO:

 * Implement lossless JPEG (ISO/IEC 10918-1:1994 aka ITU T.81 Annex H aka LJ92)
   https://bitbucket.org/baldand/mlrawviewer/src/e7abaaf4cf9be66f46e0c8844297be0e7d88c288/liblj92/?at=master
 * Create proper CinemaDNG sequences

References:

Thanks to the following for their previous work.

prng by Ben Pfaff https://benpfaff.org/writings/clc/random.html  
elphel_dng by Dave Coffin http://community.elphel.com/files/jp4/elphel_dng.c  
makeDNG from the Field project http://openendedgroup.com/field/images/makeDNG.zip  
tiff2raw by Allan G. Weber http://sipi.usc.edu/database/tiff2raw.c  
rawtiDNG by Kevin Lawson https://github.com/kelvinlawson/rawti-tools
