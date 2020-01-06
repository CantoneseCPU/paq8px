/*
  PAQ8PX file compressor/archiver
  see README for information
  see DOC for technical details
  see CHANGELOG for version history
*/

//////////////////////// Versioning ////////////////////////////////////////

#define PROGNAME     "paq8px"
#define PROGVERSION  "184"  //update version here before publishing your changes
#define PROGYEAR     "2020"

//////////////////////// Build options /////////////////////////////////////

// Uncomment one or more of the following #define-s to disable compilation of certain models/transformations
// This is useful when
// - you would like to slim the executable by eliminating unnecessary code for benchmarks where exe size matters or
// - you would like to experiment with the model-mixture
// TODO: make more models "optional"
#define USE_ZLIB
#define USE_AUDIOMODEL
#define USE_TEXTMODEL

#define NHASHCONFIG  //Remove (comment out) this line to enable hash configuration from the command line (somewhat slower compression)

//////////////////////// Debug options /////////////////////////////////////

#define NDEBUG    // Remove (comment out) this line for debugging (turns on Array bound checks and asserts)
#define NVERBOSE  // Remove (comment out) this line for more on-screen progress information

//
//  User specified options are above
//  Automatic definitions are below = for compiling no need to change anything below this line
//

//////////////////////// Target OS/Compiler ////////////////////////////////

#if defined(_WIN32) || defined(_MSC_VER)
#ifndef WINDOWS
#define WINDOWS  //to compile for Windows
#endif
#endif

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
#ifndef UNIX
#define UNIX //to compile for Unix, Linux, Solaris, MacOS / Darwin, etc)
#endif
#endif

#if !defined(WINDOWS) && !defined(UNIX)
#error Unknown target system
#endif

// Floating point operations need IEEE compliance
// Do not use compiler optimization options such as the following:
// gcc : -ffast-math (and -Ofast, -funsafe-math-optimizations, -fno-rounding-math)
// vc++: /fp:fast
#if defined(__FAST_MATH__) || defined(_M_FP_FAST) // gcc vc++
#error Avoid using aggressive floating-point compiler optimization flags
#endif

#if defined(_MSC_VER)
#define ALWAYS_INLINE  __forceinline
#elif defined(__GNUC__)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define ALWAYS_INLINE inline
#endif

#include "utils.hpp"

// Platform-independent includes
#include <sys/stat.h> //stat(), mkdir()
#include <cmath>     //floor(), sqrt()
#include <stdexcept>  //std::exception

// zlib
#ifdef USE_ZLIB

#include <zlib.h>

#endif

//////////////////// Cross-platform definitions /////////////////////////////////////

#ifdef _MSC_VER
#define fseeko(a,b,c) _fseeki64(a,b,c)
#define ftello(a) _ftelli64(a)
#else
#ifndef UNIX
#ifndef fseeko
#define fseeko(a,b,c) fseeko64(a,b,c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif

#if defined(__GNUC__) || defined(__clang__)
#define bswap(x)   __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#define bswap(x)   _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#else
#define bswap(x) \
+   ((((x) & 0xff000000) >> 24) | \
+    (((x) & 0x00ff0000) >>  8) | \
+    (((x) & 0x0000ff00) <<  8) | \
+    (((x) & 0x000000ff) << 24))
#define bswap64(x) \
+    ((x)>>56) |
+   (((x)<<40) & 0x00FF000000000000) | \
+   (((x)<<24) & 0x0000FF0000000000) | \
+   (((x)<<8 ) & 0x000000FF00000000) | \
+   (((x)>>8 ) & 0x00000000FF000000) | \
+   (((x)>>24) & 0x0000000000FF0000) | \
+   (((x)>>40) & 0x000000000000FF00) | \
+   ((x) << 56))
#endif

#include "simd.hpp"

static_assert(sizeof(uint8_t) == 1, "sizeof(uint8_t)");
static_assert(sizeof(uint16_t) == 2, "sizeof(uint16_t)");
static_assert(sizeof(uint32_t) == 4, "sizeof(uint32_t)");
static_assert(sizeof(uint64_t) == 8, "sizeof(uint64_t)");
static_assert(sizeof(short) == 2, "sizeof(short)");
static_assert(sizeof(int) == 4, "sizeof(int)");


#include "ProgramChecker.hpp"
#include "Array.hpp"
#include "String.hpp"
#include "File.hpp"
#include "Random.hpp"
#include "RingBuffer.hpp"
#include "ModelStats.hpp"
#include "Shared.hpp"
#include "Ilog.hpp"
#include "StateTable.hpp"
#include "Squash.hpp"
#include "Stretch.hpp"
#include "IPredictor.hpp"
#include "UpdateBroadcaster.hpp"
#include "Mixer.hpp"
#include "APM1.hpp"
#include "StateMap.hpp"
#include "Hash.hpp"
#include "BH.hpp"
#include "HashTable.hpp"
#include "SmallStationaryContextMap.hpp"
#include "StationaryMap.hpp"
#include "IndirectMap.hpp"
#include "ContextMap.hpp"
#include "ContextMap2.hpp"
#include "OLS.hpp"
#include "LMS.hpp"
#include "IndirectContext.hpp"
#include "MTFList.hpp"

#ifdef USE_TEXTMODEL
#include "text/Word.hpp"
#include "text/Segment.hpp"
#include "text/Sentence.hpp"
#include "text/Paragraph.hpp"
#include "text/Language.hpp"
#include "text/French.hpp"
#include "text/English.hpp"
#include "text/German.hpp"
#include "text/Stemmer.hpp"
#include "text/EnglishStemmer.hpp"
#include "text/FrenchStemmer.hpp"
#include "text/GermanStemmer.hpp"
#include "text/Entry.hpp"
#include "text/WordEmbeddingDictionary.hpp"
#endif //USE_TEXTMODEL

#include "text/TextModel.hpp"
#include "MatchModel.hpp"
#include "SparseMatchModel.hpp"
#include "CharGroupModel.hpp"
#include "WordModel.hpp"
#include "RecordModel.hpp"
#include "LinearPredictionModel.hpp"
#include "SparseModel.hpp"
#include "Image24BitModel.hpp"
#include "Image8BitModel.hpp"
#include "Image4BitModel.hpp"
#include "Image1BitModel.hpp"
#include "JpegModel.hpp"
#include "AudioModel.hpp"
#include "Audio8BitModel.hpp"
#include "Audio16BitModel.hpp"
#include "ExeModel.hpp"
#include "IndirectModel.hpp"
#include "Dmc.hpp"
#include "NestModel.hpp"
#include "XMLModel.hpp"
#include "NormalModel.hpp"
#include "Models.hpp"
#include "ContextModel.hpp"
#include "SSE.hpp"
#include "Predictor.hpp"
#include "Encoder.hpp"
#include "Filters.hpp"
#include "ListOfFiles.hpp"

int main_utf8(int argc, char **argv) {
  try {

    if( !toScreen ) //we need a minimal feedback when redirected
      fprintf(stderr, PROGNAME " archiver v" PROGVERSION " (c) " PROGYEAR ", Matt Mahoney et al.\n");
    printf(PROGNAME " archiver v" PROGVERSION " (c) " PROGYEAR ", Matt Mahoney et al.\n");

    // Print help message
    if( argc < 2 ) {
      printf("\n"
             "Free under GPL, http://www.gnu.org/licenses/gpl.txt\n\n"
             "To compress:\n"
             "\n"
             "  " PROGNAME " -LEVEL[SWITCHES] INPUTSPEC [OUTPUTSPEC]\n"
             "\n"
             "    -LEVEL:\n"
             "      -0 = store (uses 322 MB)\n"
             "      -1 -2 -3 = faster (uses 417, 431, 459 MB)\n"
             "      -4 -5 -6 -7 -8 -9 = smaller (uses 514, 624, 845, 1288, 2172, 3941 MB)\n"
             "    The listed memory requirements are indicative, actual usage may vary\n"
             "    depending on several factors including need for temporary files,\n"
             "    temporary memory needs of some preprocessing (transformations), etc.\n"
             "\n"
             "    Optional compression SWITCHES:\n"
             "      b = Brute-force detection of DEFLATE streams\n"
             "      e = Pre-train x86/x64 model\n"
             "      t = Pre-train main model with word and expression list\n"
             "          (english.dic, english.exp)\n"
             "      a = Adaptive learning rate\n"
             "      s = Skip the color transform, just reorder the RGB channels\n"
             "    INPUTSPEC:\n"
             "    The input may be a FILE or a PATH/FILE or a [PATH/]@FILELIST.\n"
             "    Only file content and the file size is kept in the archive. Filename,\n"
             "    path, date and any file attributes or permissions are not stored.\n"
             "    When a @FILELIST is provided the FILELIST file will be considered\n"
             "    implicitly as the very first input file. It will be compressed and upon\n"
             "    decompression it will be extracted. The FILELIST is a tab separated text\n"
             "    file where the first column contains the names and optionally the relative\n"
             "    paths of the files to be compressed. The paths should be relative to the\n"
             "    FILELIST file. In the other columns you may store any information you wish\n"
             "    to keep about the files (timestamp, owner, attributes or your own remarks).\n"
             "    These extra columns will be ignored by the compressor and the decompressor\n"
             "    but you may restore full file information using them with a 3rd party\n"
             "    utility. The FILELIST file must contain a header but will be ignored.\n"
             "\n"
             "    OUTPUTSPEC:\n"
             "    When omitted: the archive will be created in the same folder where the\n"
             "    input file resides. The archive filename will be constructed from the\n"
             "    input file name by appending ." PROGNAME PROGVERSION " extension\n"
             "    to it.\n"
             "    When OUTPUTSPEC is a filename (with an optional path) it will be\n"
             "    used as the archive filename.\n"
             "    When OUTPUTSPEC is a folder the archive file will be generated from\n"
             "    the input filename and will be created in the specified folder.\n"
             "    If the archive file already exists it will be overwritten.\n"
             "\n"
             "    Examples:\n"
             "      " PROGNAME " -8 enwik8\n"
             "      " PROGNAME " -8ba b64sample.xml\n"
             "      " PROGNAME " -8 @myfolder/myfilelist.txt\n"
             "      " PROGNAME " -8a benchmark/enwik8 results/enwik8_a_" PROGNAME PROGVERSION "\n"
             "\n"
             "To extract (decompress contents):\n"
             "\n"
             "  " PROGNAME " -d [INPUTPATH/]ARCHIVEFILE [[OUTPUTPATH/]OUTPUTFILE]\n"
             "    If an output folder is not provided the output file will go to the input\n"
             "    folder. If an output filename is not provided output filename will be the\n"
             "    same as ARCHIVEFILE without the last extension (e.g. without ." PROGNAME PROGVERSION")\n"
             "    When OUTPUTPATH does not exist it will be created.\n"
             "    When the archive contains multiple files, first the @LISTFILE is extracted\n"
             "    then the rest of the files. Any required folders will be created.\n"
             "\n"
             "To test:\n"
             "\n"
             "  " PROGNAME " -t [INPUTPATH/]ARCHIVEFILE [[OUTPUTPATH/]OUTPUTFILE]\n"
             "    Tests contents of the archive by decompressing it (to memory) and comparing\n"
             "    the result to the original file(s). If a file fails the test, the first\n"
             "    mismatched position will be printed to screen.\n"
             "\n"
             "To list archive contents:\n"
             "\n"
             "  " PROGNAME " -l [INPUTFOLDER/]ARCHIVEFILE\n"
             "    Extracts @FILELIST from archive (to memory) and prints its content\n"
             "    to screen. This command is only applicable to multi-file archives.\n"
             "\n"
             "Additional optional swithes:\n"
             "\n"
             "    -v\n"
             "    Print more detailed (verbose) information to screen.\n"
             "\n"
             "    -log LOGFILE\n"
             "    Logs (appends) compression results in the specified tab separated LOGFILE.\n"
             "    Logging is only applicable for compression.\n"
             "\n"
             "    -simd [NONE|SSE2|AVX2]\n"
             "    Overrides detected SIMD instruction set for neural network operations\n"
             "\n"
             "Remark: the command line arguments may be used in any order except the input\n"
             "and output: always the input comes first then (the optional) output.\n"
             "\n"
             "    Example:\n"
             "      " PROGNAME " -8 enwik8 folder/ -v -log logfile.txt -simd sse2\n"
             "    is equivalent to:\n"
             "      " PROGNAME " -v -simd sse2 enwik8 -log logfile.txt folder/ -8\n");
      quit();
    }

    // Parse command line arguments
    typedef enum { doNone, doCompress, doExtract, doCompare, doList } WHATTODO;
    WHATTODO whattodo = doNone;
    bool verbose = false;
    int c;
    int simdIset = -1; //simd instruction set to use

    FileName input;
    FileName output;
    FileName inputpath;
    FileName outputpath;
    FileName archiveName;
    FileName logfile;
    String hashconfig;

    for( int i = 1; i < argc; i++ ) {
      int arg_len = (int) strlen(argv[i]);
      if( argv[i][0] == '-' ) {
        if( arg_len == 1 )
          quit("Empty command.");
        if( argv[i][1] >= '0' && argv[i][1] <= '9' ) {
          if( whattodo != doNone )
            quit("Only one command may be specified.");
          whattodo = doCompress;
          level = argv[i][1] - '0';
          //process optional compression switches
          for( int j = 2; j < arg_len; j++ ) {
            switch( argv[i][j] & 0xDFU ) {
              case 'B':
                options |= OPTION_BRUTE;
                break;
              case 'E':
                options |= OPTION_TRAINEXE;
                break;
              case 'T':
                options |= OPTION_TRAINTXT;
                break;
              case 'A':
                options |= OPTION_ADAPTIVE;
                break;
              case 'S':
                options |= OPTION_SKIPRGB;
                break;
              default: {
                printf("Invalid compression switch: %c", argv[1][j]);
                quit();
              }
            }
          }
        } else if( strcasecmp(argv[i], "-d") == 0 ) {
          if( whattodo != doNone )
            quit("Only one command may be specified.");
          whattodo = doExtract;
        } else if( strcasecmp(argv[i], "-t") == 0 ) {
          if( whattodo != doNone )
            quit("Only one command may be specified.");
          whattodo = doCompare;
        } else if( strcasecmp(argv[i], "-l") == 0 ) {
          if( whattodo != doNone )
            quit("Only one command may be specified.");
          whattodo = doList;
        } else if( strcasecmp(argv[i], "-v") == 0 ) {
          verbose = true;
        } else if( strcasecmp(argv[i], "-log") == 0 ) {
          if( logfile.strsize() != 0 )
            quit("Only one logfile may be specified.");
          if( ++i == argc )
            quit("The -log switch requires a filename.");
          logfile += argv[i];
        }
#ifndef NHASHCONFIG
          else if (strcasecmp(argv[i],"-hash")==0) {
            if(hashconfig.strsize()!=0)quit("Only one hash configuration may be specified.");
            if(++i==argc)quit("The -hash switch requires more parameters: 14 magic hash constants delimited by non-spaces.");
            hashconfig+=argv[i];
            loadHashesFromCmd(hashconfig.c_str());
          }
#endif
        else if( strcasecmp(argv[i], "-simd") == 0 ) {
          if( ++i == argc )
            quit("The -simd switch requires an instruction set name (NONE,SSE2,AVX2).");
          if( strcasecmp(argv[i], "NONE") == 0 )
            simdIset = 0;
          else if( strcasecmp(argv[i], "SSE2") == 0 )
            simdIset = 3;
          else if( strcasecmp(argv[i], "AVX2") == 0 )
            simdIset = 9;
          else
            quit("Invalid -simd option. Use -simd NONE, -simd SSE2 or -simd AVX2.");
        } else {
          printf("Invalid command: %s", argv[i]);
          quit();
        }
      } else { //this parameter does not begin with a dash ("-") -> it must be a folder/filename
        if( input.strsize() == 0 ) {
          input += argv[i];
          input.replaceSlashes();
        } else if( output.strsize() == 0 ) {
          output += argv[i];
          output.replaceSlashes();
        } else
          quit("More than two filenames specified. Only an input and an output is needed.");
      }
    }

    if( verbose ) {
      //print compiled-in modules
      printf("\n");
      printf("Build: ");
#ifdef USE_ZLIB
      printf("USE_ZLIB ");
#else
      printf("NO_ZLIB ");
#endif

#ifdef USE_AUDIOMODEL
      printf("USE_AUDIOMODEL ");
#else
      printf("NO_WAVMODEL ");
#endif

#ifdef USE_TEXTMODEL
      printf("USE_TEXTMODEL ");
#else
      printf("NO_TEXTMODEL ");
#endif

      printf("\n");
    }

    // Determine CPU's (and OS) support for SIMD vectorization istruction set
    int detectedSimdIset = simdDetect();
    if( simdIset == -1 )
      simdIset = detectedSimdIset;
    if( simdIset > detectedSimdIset )
      printf("\nOverriding system highest vectorization support. Expect a crash.");

    // Print anything only if the user wants/needs to know
    if( verbose || simdIset != detectedSimdIset ) {
      printf("\nHighest SIMD vectorization support on this system: ");
      if( detectedSimdIset < 0 || detectedSimdIset > 9 )
        quit("Oops, sorry. Unexpected result.");
      static const char *vectorizationString[10] = {"None", "MMX", "SSE", "SSE2", "SSE3", "SSSE3", "SSE4.1", "SSE4.2", "AVX", "AVX2"};
      printf("%s.\n", vectorizationString[detectedSimdIset]);

      printf("Using ");
      if( simdIset >= 9 )
        printf("AVX2");
      else if( simdIset >= 3 )
        printf("SSE2");
      else
        printf("non-vectorized");
      printf(" neural network functions.\n");
    }

    // Set highest or user selected vectorization mode
    if( simdIset >= 9 )
      MixerFactory::setSimd(SIMD_AVX2);
    else if( simdIset >= 3 )
      MixerFactory::setSimd(SIMD_SSE2);
    else
      MixerFactory::setSimd(SIMD_NONE);

    if( verbose ) {
      printf("\n");
      printf(" Command line   =");
      for( int i = 0; i < argc; i++ )
        printf(" %s", argv[i]);
      printf("\n");
    }

    // Successfully parsed command line arguments
    // Let's check their validity
    if( whattodo == doNone )
      quit("A command switch is required: -0..-9 to compress, -d to decompress, -t to test, -l to list.");
    if( input.strsize() == 0 ) {
      printf("\nAn %s is required %s.\n", whattodo == doCompress ? "input file or filelist" : "archive filename",
             whattodo == doCompress ? "for compressing" : whattodo == doExtract ? "for decompressing" : whattodo == doCompare
                                                                                                        ? "for testing" : whattodo == doList
                                                                                                                          ? "to list its contents"
                                                                                                                          : "");
      quit();
    }
    if( whattodo == doList && output.strsize() != 0 )
      quit("The list command needs only one file parameter.");

    // File list supplied?
    if( input.beginsWith("@")) {
      if( whattodo == doCompress ) {
        options |= OPTION_MULTIPLE_FILE_MODE;
        input.stripStart(1);
      } else
        quit("A file list (a file name prefixed by '@') may only be specified when compressing.");
    }

    int pathtype;

    //Logfile supplied?
    if( logfile.strsize() != 0 ) {
      if( whattodo != doCompress )
        quit("A log file may only be specified for compression.");
      pathtype = examinePath(logfile.c_str());
      if( pathtype == 2 || pathtype == 4 )
        quit("Specified log file should be a file not a directory.");
      if( pathtype == 0 ) {
        printf("\nThere is a problem with the log file: %s", logfile.c_str());
        quit();
      }
    }

    // Separate paths from input filename/directory name
    pathtype = examinePath(input.c_str());
    if( pathtype == 2 || pathtype == 4 ) {
      printf("\nSpecified input is a directory but should be a file: %s", input.c_str());
      quit();
    }
    if( pathtype == 3 ) {
      printf("\nSpecified input file does not exist: %s", input.c_str());
      quit();
    }
    if( pathtype == 0 ) {
      printf("\nThere is a problem with the specified input file: %s", input.c_str());
      quit();
    }
    if( input.lastSlashPos() >= 0 ) {
      inputpath += input.c_str();
      inputpath.keepPath();
      input.keepFilename();
    }

    // Separate paths from output filename/directory name
    if( output.strsize() > 0 ) {
      pathtype = examinePath(output.c_str());
      if( pathtype == 1 || pathtype == 3 ) { //is an existing file, or looks like a file
        if( output.lastSlashPos() >= 0 ) {
          outputpath += output.c_str();
          outputpath.keepPath();
          output.keepFilename();
        }
      } else if( pathtype == 2 || pathtype == 4 ) {//is an existing directory, or looks like a directory
        outputpath += output.c_str();
        if( !outputpath.endswith("/") && !outputpath.endswith("\\"))
          outputpath += GOODSLASH;
        //output file is not specified
        output.resize(0);
        output.pushBack(0);
      } else {
        printf("\nThere is a problem with the specified output: %s", output.c_str());
        quit();
      }
    }

    //determine archive name
    if( whattodo == doCompress ) {
      archiveName += outputpath.c_str();
      if( output.strsize() == 0 ) { // If no archive name is provided, construct it from input (append PROGNAME extension to input filename)
        archiveName += input.c_str();
        archiveName += "." PROGNAME PROGVERSION;
      } else
        archiveName += output.c_str();
    } else { // extract/compare/list: archivename is simply the input
      archiveName += inputpath.c_str();
      archiveName += input.c_str();
    }
    if( verbose ) {
      printf(" Archive        = %s\n", archiveName.c_str());
      printf(" Input folder   = %s\n", inputpath.strsize() == 0 ? "." : inputpath.c_str());
      printf(" Output folder  = %s\n", outputpath.strsize() == 0 ? "." : outputpath.c_str());
    }

    Mode mode = whattodo == doCompress ? COMPRESS : DECOMPRESS;

    ListOfFiles listoffiles;

    //set basePath for filelist
    listoffiles.setBasePath(whattodo == doCompress ? inputpath.c_str() : outputpath.c_str());

    // Process file list (in multiple file mode)
    if( options & OPTION_MULTIPLE_FILE_MODE ) { //multiple file mode
      assert(whattodo == doCompress);
      // Read and parse filelist file
      FileDisk f;
      FileName fn(inputpath.c_str());
      fn += input.c_str();
      f.open(fn.c_str(), true);
      while( true ) {
        c = f.getchar();
        listoffiles.addChar(c);
        if( c == EOF)
          break;
      }
      f.close();
      //Verify input files
      for( int i = 0; i < listoffiles.getCount(); i++ )
        getfileSize(listoffiles.getfilename(i)); // Does file exist? Is it readable? (we don't actually need the filesize now)
    } else { //single file mode or extract/compare/list
      FileName fn(inputpath.c_str());
      fn += input.c_str();
      getfileSize(fn.c_str()); // Does file exist? Is it readable? (we don't actually need the filesize now)
    }

    FileDisk archive;  // compressed file

    if( mode == DECOMPRESS ) {
      archive.open(archiveName.c_str(), true);
      // Verify archive header, get level and options
      int len = (int) strlen(PROGNAME);
      for( int i = 0; i < len; i++ )
        if( archive.getchar() != PROGNAME[i] ) {
          printf("%s: not a valid %s file.", archiveName.c_str(), PROGNAME);
          quit();
        }
      level = archive.getchar();
      c = archive.getchar();
      if( c == EOF)
        printf("Unexpected end of archive file.\n");
      options = (uint8_t) c;
    }

    if( verbose ) {
      // Print specified command
      printf(" To do          = ");
      if( whattodo == doNone )
        printf("-");
      if( whattodo == doCompress )
        printf("Compress");
      if( whattodo == doExtract )
        printf("Extract");
      if( whattodo == doCompare )
        printf("Compare");
      if( whattodo == doList )
        printf("List");
      printf("\n");
      // Print specified options
      printf(" Level          = %d\n", level);
      printf(" Brute      (b) = %s\n", options & OPTION_BRUTE ? "On  (Brute-force detection of DEFLATE streams)"
                                                              : "Off"); //this is a compression-only option, but we put/get it for reproducibility
      printf(" Train exe  (e) = %s\n", options & OPTION_TRAINEXE ? "On  (Pre-train x86/x64 model)" : "Off");
      printf(" Train txt  (t) = %s\n", options & OPTION_TRAINTXT ? "On  (Pre-train main model with word and expression list)" : "Off");
      printf(" Adaptive   (a) = %s\n", options & OPTION_ADAPTIVE ? "On  (Adaptive learning rate)" : "Off");
      printf(" Skip RGB   (s) = %s\n", options & OPTION_SKIPRGB ? "On  (Skip the color transform, just reorder the RGB channels)" : "Off");
      printf(" File mode      = %s\n", options & OPTION_MULTIPLE_FILE_MODE ? "Multiple" : "Single");
    }
    printf("\n");

    int number_of_files = 1; //default for single file mode

    // Write archive header to archive file
    if( mode == COMPRESS ) {
      if( options & OPTION_MULTIPLE_FILE_MODE ) { //multiple file mode
        number_of_files = listoffiles.getCount();
        printf("Creating archive %s in multiple file mode with %d file%s...\n", archiveName.c_str(), number_of_files,
               number_of_files > 1 ? "s" : "");
      } else //single file mode
        printf("Creating archive %s in single file mode...\n", archiveName.c_str());
      archive.create(archiveName.c_str());
      archive.append(PROGNAME);
      archive.putChar(level);
      archive.putChar(options);
    }

    // In single file mode with no output filename specified we must construct it from the supplied archive filename
    if((options & OPTION_MULTIPLE_FILE_MODE) == 0 ) { //single file mode
      if((whattodo == doExtract || whattodo == doCompare) && output.strsize() == 0 ) {
        output += input.c_str();
        const char *file_extension = "." PROGNAME PROGVERSION;
        if( output.endswith(file_extension))
          output.stripEnd((int) strlen(file_extension));
        else {
          printf("Can't construct output filename from archive filename.\nArchive file extension must be: '%s'", file_extension);
          quit();
        }
      }
    }

    // Set globals according to requested compression level
    assert(level >= 0 && level <= 9);
    Encoder en(mode, &archive);
    uint64_t content_size = 0;
    uint64_t total_size = 0;

    // Compress list of files
    if( mode == COMPRESS ) {
      uint64_t start = en.size(); //header size (=8)
      if( verbose )
        printf("Writing header : %" PRIu64 " bytes\n", start);
      total_size += start;
      if((options & OPTION_MULTIPLE_FILE_MODE) != 0 ) { //multiple file mode

        en.compress(TEXT);
        uint64_t len1 = input.size(); //ASCIIZ filename of listfile - with ending zero
        const String *const s = listoffiles.getString();
        uint64_t len2 = s->size(); //ASCIIZ filenames of files to compress - with ending zero
        en.encodeBlockSize(len1 + len2);

        for( uint64_t i = 0; i < len1; i++ )
          en.compress(input[i]); //ASCIIZ filename of listfile
        for( uint64_t i = 0; i < len2; i++ )
          en.compress((*s)[i]); //ASCIIZ filenames of files to compress

        printf("1/2 - Filename of listfile : %" PRIu64 " bytes\n", len1);
        printf("2/2 - Content of listfile  : %" PRIu64 " bytes\n", len2);
        printf("----- Compressed to        : %" PRIu64 " bytes\n", en.size() - start);
        total_size += len1 + len2;
      }
    }

    // Decompress list of files
    if( mode == DECOMPRESS && (options & OPTION_MULTIPLE_FILE_MODE) != 0 ) {
      const char *errmsg_invalid_char = "Invalid character or unexpected end of archive file.";
      // name of listfile
      FileName list_filename(outputpath.c_str());
      if( output.strsize() != 0 )
        quit("Output filename must not be specified when extracting multiple files.");
      if((c = en.decompress()) != TEXT )
        quit(errmsg_invalid_char);
      en.decodeBlockSize(); //we don't really need it
      while((c = en.decompress()) != 0 ) {
        if( c == 255 )
          quit(errmsg_invalid_char);
        list_filename += (char) c;
      }
      while((c = en.decompress()) != 0 ) {
        if( c == 255 )
          quit(errmsg_invalid_char);
        listoffiles.addChar((char) c);
      }
      if( whattodo == doList )
        printf("File list of %s archive:\n", archiveName.c_str());

      number_of_files = listoffiles.getCount();

      //write filenames to screen or listfile or verify (compare) contents
      if( whattodo == doList )
        printf("%s\n", listoffiles.getString()->c_str());
      else if( whattodo == doExtract ) {
        FileDisk f;
        f.create(list_filename.c_str());
        String *s = listoffiles.getString();
        f.blockWrite((uint8_t *) (&(*s)[0]), s->strsize());
        f.close();
      } else if( whattodo == doCompare ) {
        FileDisk f;
        f.open(list_filename.c_str(), true);
        String *s = listoffiles.getString();
        for( uint64_t i = 0; i < s->strsize(); i++ )
          if( f.getchar() != (uint8_t) (*s)[i] )
            quit("Mismatch in list of files.");
        if( f.getchar() != EOF)
          printf("Filelist on disk is larger than in archive.\n");
        f.close();
      }
    }

    if( whattodo == doList && (options & OPTION_MULTIPLE_FILE_MODE) == 0 )
      quit("Can't list. Filenames are not stored in single file mode.\n");

    // Compress or decompress files
    if( mode == COMPRESS ) {
      if( !toScreen ) //we need a minimal feedback when redirected
        fprintf(stderr, "Output is redirected - only minimal feedback is on screen\n");
      if((options & OPTION_MULTIPLE_FILE_MODE) != 0 ) { //multiple file mode
        for( int i = 0; i < number_of_files; i++ ) {
          const char *fname = listoffiles.getfilename(i);
          uint64_t fsize = getfileSize(fname);
          if( !toScreen ) //we need a minimal feedback when redirected
            fprintf(stderr, "\n%d/%d - Filename: %s (%" PRIu64 " bytes)\n", i + 1, number_of_files, fname, fsize);
          printf("\n%d/%d - Filename: %s (%" PRIu64 " bytes)\n", i + 1, number_of_files, fname, fsize);
          compressfile(fname, fsize, en, verbose);
          total_size += fsize + 4; //4: file size information
          content_size += fsize;
        }
      } else { //single file mode
        FileName fn;
        fn += inputpath.c_str();
        fn += input.c_str();
        const char *fname = fn.c_str();
        uint64_t fsize = getfileSize(fname);
        if( !toScreen ) //we need a minimal feedback when redirected
          fprintf(stderr, "\nFilename: %s (%" PRIu64 " bytes)\n", fname, fsize);
        printf("\nFilename: %s (%" PRIu64 " bytes)\n", fname, fsize);
        compressfile(fname, fsize, en, verbose);
        total_size += fsize + 4; //4: file size information
        content_size += fsize;
      }

      uint64_t pre_flush = en.size();
      en.flush();
      total_size += en.size() - pre_flush; //we consider padding bytes as auxiliary bytes
      printf("-----------------------\n");
      printf("Total input size     : %" PRIu64 "\n", content_size);
      if( verbose )
        printf("Total metadata bytes : %" PRIu64 "\n", total_size - content_size);
      printf("Total archive size   : %" PRIu64 "\n", en.size());
      printf("\n");
      // Log compression results
      if( logfile.strsize() != 0 ) {
        String results;
        pathtype = examinePath(logfile.c_str());
        //Write header if needed
        if( pathtype == 3 /*does not exist*/ ||
            (pathtype == 1 && getfileSize(logfile.c_str()) == 0)/*exists but does not contain a header*/)
          results += "PROG_NAME\tPROG_VERSION\tCOMMAND_LINE\tLEVEL\tINPUT_FILENAME\tORIGINAL_SIZE_BYTES\tCOMPRESSED_SIZE_BYTES\tRUNTIME_MS\n";
        //Write results to logfile
        results += PROGNAME "\t" PROGVERSION "\t";
        for( int i = 1; i < argc; i++ ) {
          if( i != 0 )
            results += ' ';
          results += argv[i];
        }
        results += "\t";
        results += uint64_t(level);
        results += "\t";
        results += input.c_str();
        results += "\t";
        results += content_size;
        results += "\t";
        results += en.size();
        results += "\t";
        results += uint64_t(programChecker.getRuntime() * 1000.0);
        results += "\t";
        results += "\n";
        appendToFile(logfile.c_str(), results.c_str());
        printf("Results logged to file '%s'\n", logfile.c_str());
        printf("\n");
      }
    } else { //decompress
      if( whattodo == doExtract || whattodo == doCompare ) {
        FMode fmode = whattodo == doExtract ? FDECOMPRESS : FCOMPARE;
        if((options & OPTION_MULTIPLE_FILE_MODE) != 0 ) { //multiple file mode
          for( int i = 0; i < number_of_files; i++ ) {
            const char *fname = listoffiles.getfilename(i);
            decompressFile(fname, fmode, en);
          }
        } else { //single file mode
          FileName fn;
          fn += outputpath.c_str();
          fn += output.c_str();
          const char *fname = fn.c_str();
          decompressFile(fname, fmode, en);
        }
      }
    }

    archive.close();
    if( whattodo != doList )
      programChecker.print();
  }
    // we catch only the intentional exceptions from quit() to exit gracefully
    // any other exception should result in a crash and must be investigated
  catch( IntentionalException const & ) {
  }

  return 0;
}

int main(int argc, char **argv) {
#ifdef WINDOWS
  // On Windows, argv is encoded in the effective codepage, therefore unsuitable for acquiring command line arguments (file names
  // in our case) not representable in that particular codepage.
  // -> We will recreate argv as UTF8 (like in Linux)
  uint32_t oldcp = GetConsoleOutputCP();
  SetConsoleOutputCP(CP_UTF8);
  wchar_t **szArglist;
  int argc_utf8;
  char** argv_utf8;
  if( (szArglist = CommandLineToArgvW(GetCommandLineW(), &argc_utf8)) == NULL) {
    printf("CommandLineToArgvW failed\n");
    return 0;
  } else {
    if(argc!=argc_utf8)quit("Error preparing command line arguments.");
    argv_utf8=new char*[argc_utf8+1];
    for(int i=0; i<argc_utf8; i++) {
      wchar_t *s=szArglist[i];
      int buffersize = WideCharToMultiByte(CP_UTF8,0,s,-1,NULL,0,NULL,NULL);
      argv_utf8[i] = new char[buffersize];
      WideCharToMultiByte(CP_UTF8,0,s,-1,argv_utf8[i],buffersize,NULL,NULL);
      //printf("%d: %s\n", i, argv_utf8[i]); //debug: see if conversion is successful
    }
    argv_utf8[argc_utf8]=nullptr;
    int retval=main_utf8(argc_utf8, argv_utf8);
    for(int i=0; i<argc_utf8; i++)
      delete[] argv_utf8[i];
    delete[] argv_utf8;
    SetConsoleOutputCP(oldcp);
    return retval;
  }
#else
  return main_utf8(argc, argv);
#endif
}
