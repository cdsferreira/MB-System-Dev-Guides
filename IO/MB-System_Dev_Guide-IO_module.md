# Developer's Guide to Coding an MB-System I/O Module: Native GSF Reader (Version 4)

This guide explains how to build a new MB-System I/O module that reads Generic Sensor Format (GSF) data directly, without linking against the external `libgsf` library. The result is a native reader for format id 300, format name `GSFNATIV`, using the `gsfnative` data system.

The guide is written for readers who understand general programming concepts but are newer to C and to MB-System internals. It assumes you may be comfortable with languages such as Python, where memory management, pointer passing, and binary parsing are usually hidden behind libraries. Here, those details are made explicit so you can see how MB-System modules are actually wired together.

This version 4 guide is a clean teaching document. It keeps the validated logic from the supplied source files, but removes the version-by-version editorial discussion so the document reads as a standalone implementation guide.

## What this module does

MB-System uses a common library called MBIO to read and write many sonar formats through one API. Each format module plugs into that common API by registering a set of C functions that MB-System can call for opening files, reading records, extracting bathymetry, and so on.

This module adds a second GSF path alongside MB-System's existing format 121. Format 121 reads GSF through the external Leidos `libgsf` library, while this native module parses the documented GSF byte stream directly and uses MB-System's own buffered file I/O layer (`MBFILETYPE_SINGLE`).

## How to read this guide

The implementation is split across three source files, and this separation is important. If you come from Python, think of this as one small package with a data model file, a data-model implementation file, and a file-format adapter file.

- `mbsys_gsfnative.h` defines the C structures and function prototypes.
- `mbsys_gsfnative.c` implements the data-system functions such as allocation, extraction, insertion, altitude extraction, and copying.
- `mbr_gsfnativ.c` implements the format-level reader and registers the module with MBIO.

## MB-System architecture

In MB-System, an I/O module has two conceptual layers. The **data system** describes how one decoded record lives in memory, while the **format reader** knows how to translate bytes on disk into that in-memory structure.

In Python, you might imagine the data system as a class definition and the format reader as a parser function that fills an instance of that class. In C, there are no classes, so this is done with `struct` definitions plus functions that operate on pointers to those structures.

### The central MBIO object

When MB-System opens a file, it creates a large control structure called `struct mb_io_struct`, usually accessed through a pointer named `mbioptr`. That structure stores the format id, function pointers for the active module, scratch buffers, and file I/O state.

A pointer is just a variable that stores a memory address. If Python variables feel like references to objects, a C pointer is the low-level version of that idea, except you must manage types and memory much more explicitly.

### Why function pointers matter

MB-System's generic read functions do not know ahead of time whether a file is Kongsberg, Reson, GSF, or something else. Instead, when a format is registered, MB-System fills in function pointers inside `mbioptr`, so later calls dispatch to the correct implementation for the current format.

If you have used a Python dictionary that maps strings to callable functions, this is the same overall idea. In C, the function address is stored in a typed slot inside a struct instead of in a high-level container.

## The three files

### Header file

`mbsys_gsfnative.h` declares constants, record identifiers, array limits, data structures, and function prototypes. Header files in C are shared declarations: they tell the compiler what names and signatures exist so multiple `.c` files can agree on the same interface.

You can think of a header file as similar to a Python type stub or a module interface, except C requires the declarations before use. It is normal for a `.h` file to contain `#define` constants and `struct` layouts, but not most executable logic.

### Data-system file

`mbsys_gsfnative.c` implements the functions that operate on decoded records once they are already in memory. These include allocation, extraction of bathymetry and navigation, insertion of values, travel-time stubs, gain stubs, and copying.

This file is where GSF values are translated into MB-System's common representation. For example, native per-beam beam flags are mapped into MB-System's `beamflag` values, and decoded ping fields are converted into navigation, heading, and depth outputs that other MB-System programs understand.

### Format-reader file

`mbr_gsfnativ.c` is responsible for reading bytes from disk, interpreting GSF record headers, dispatching by record type, and decoding the records that are in scope. It also registers the module's function pointers and sets the format metadata that MB-System uses.

If the data-system file is the in-memory data model, the format-reader file is the binary parser. This is the file to study first if you want to understand how bytes become meaningful values.

## C concepts for non-C readers

### Structs

A C `struct` is a fixed memory layout that groups fields together. Unlike a Python object or dictionary, a C struct has a fixed layout known at compile time, so each field has a defined type and byte size.

For binary file work, that predictability is useful. It lets you create in-memory containers for decoded data, while still controlling exactly how individual bytes are read and converted.

### Pointers and `void *`

Many MB-System function signatures take `void *` arguments such as `void *mbioptr` or `void *storeptr`. A `void *` is a generic pointer that can point to anything, but before using it meaningfully the code casts it back to the correct pointer type.

This is somewhat like accepting an `object` in Python and then checking or assuming its true type before accessing attributes. C gives you flexibility, but also less safety, so the cast must be correct.

### Memory allocation

In Python, objects are created automatically and reclaimed by the garbage collector. In C, code must explicitly allocate and free memory, which is why this module uses MB-System helpers such as `mb_mallocd`, `mb_reallocd`, and `mb_freed`.

The `mbsys_gsfnativealloc()` function allocates the main storage struct, while `mbralmgsfnativ()` also sets up a reusable read buffer stored in `mbioptr->saveptr1`. Reusing one growable buffer avoids repeated allocation for every record read.

### Arrays

C arrays are fixed-size contiguous memory blocks. This module uses fixed upper bounds such as `MBSYSGSFNTVMAXBEAMS` and `MBSYSGSFNTVMAXATTITUDE`, then stores only as many entries as are valid for the current record.

That design is common in scientific C code because it is simple and fast, even if it is less dynamic than Python lists. The code still checks declared counts defensively so malformed input does not run past array bounds.

### Endianness and byte swapping

GSF stores multi-byte fields in big-endian order. Many modern systems are little-endian, so the reader must know when to swap bytes while decoding integers, shorts, and floats.

The validated v4 code computes this once using `mb_swap_check()` and stores the result in `mbioptr->byteswapped`. That cached flag is then passed into every `mb_get_binary_int()`, `mb_get_binary_short()`, and `mb_get_binary_float()` call so values are interpreted correctly on any host.

## GSF record model

Every GSF record starts with a 4-byte size word followed by a 4-byte identifier word. If the checksum bit is set in the identifier word, a 4-byte checksum follows before the record body.

The v4 module correctly detects that checksum word and skips it when present, even though checksum verification itself is not implemented in this read-only version. This matters because failing to skip it would shift all subsequent byte offsets and corrupt the decode of the rest of the record.

### Record types in scope

This module fully decodes three top-level GSF record types: `HEADER` (1), `SWATH_BATHYMETRY_PING` (2), and `ATTITUDE` (12). Other record types are still consumed correctly by length so file offsets remain synchronized.

In v4, skipped record types are mapped to real MB-System kinds already defined in `mbstatus.h`, such as `MBDATACOMMENT`, `MBDATAHISTORY`, `MBDATASUMMARY`, and `MBDATAVELOCITYPROFILE`, rather than to a nonexistent generic `MBDATAOTHER` constant.

### Ping subrecords in scope

Inside a swath bathymetry ping, the reader decodes `SCALEFACTORS` (100), `DEPTHARRAY` (1), `ACROSSTRACKARRAY` (2), `ALONGTRACKARRAY` (3), `MEANCALAMPLITUDEARRAY` (6), and `BEAMFLAGSARRAY` (16). All other ping subrecords are skipped by their declared size.

This selective decoding keeps the first version manageable while still providing the core information MB-System needs for bathymetry, amplitude, and navigation-style extraction.

## Data structures

### Header record

The header record is simple: it stores a 12-byte version string such as `GSF-v03.11` in `struct mbsysgsfntvheaderstruct`. Since this is text rather than a binary integer, no byte swapping is needed.

### Ping header

The ping header is 56 bytes long and includes time, longitude, latitude, beam count, beam-center index, ping flags, draft-related corrections, attitude angles, speed, and several vertical reference fields. The validated reader decodes these fields in order and then converts the raw integer forms into directly usable doubles such as decimal degrees and knots.

For example, longitude and latitude are stored as signed integers in units of `1e-7` degrees, while heading is stored in hundredths of a degree. In C, these conversions are explicit arithmetic operations, not hidden properties, which makes the decode logic easy to inspect.

### Scale factors

Some GSF arrays are stored as scaled integers rather than physical values. The scale-factor subrecord provides a multiplier and offset for specific subrecord ids, and the helper `mbsysgsfnativedecodescaled()` reconstructs real values using:

\[
\text{decoded} = \frac{\text{raw}}{\text{multiplier}} - \text{offset}
\]

That pattern may look unusual if you are used to simpler `raw * scale + offset` formulas. Here the subtraction after division matches the documented purpose of the GSF offset field and the behavior expected by the validated implementation.

### Decoded ping structure

The main ping structure stores both the raw header and the decoded per-beam arrays. Keeping both is useful because MB-System sometimes needs exact original values for later processing, while higher-level extraction functions want already-decoded doubles.

This is similar to keeping both a raw JSON payload and a cleaned pandas DataFrame in Python. The raw form preserves provenance, while the decoded form is easier to use.

### Attitude structure

The attitude record stores a base time plus per-sample offsets and arrays for pitch, roll, heave, and heading. The reader converts these into per-sample times and also feeds the samples into MB-System's attitude interpolation buffer using `mbattintadd()`.

That extra step means other MB-System logic can later interpolate roll, pitch, and heave onto arbitrary timestamps. Conceptually, it is like populating a time-series cache as records are read.

## Reading flow

### Registration

The first job of `mbrregistergsfnativ()` is to install this module's function pointers into `mbioptr`. It calls `mbrinfogsfnativ()` to fill metadata such as format name, file type, beam counts, source kinds, and beamwidth defaults.

The validated v4 signature of `mbrinfogsfnativ()` matches MB-System's actual expected prototype, including `platformsource`, `sensordepthsource`, and `attitudesource`. This matters because a mismatched function signature can compile badly or cause subtle runtime problems in C.

### Allocation

`mbralmgsfnativ()` sets up the module for reading. It initializes the reusable body buffer, computes the byte-swap flag, and allocates the data-system storage structure.

This is a common C pattern: do setup work once during initialization so inner read loops stay simpler and faster.

### Top-level record read

`mbrrtgsfnativ()` is the top-level MBIO entry point for one read operation. It calls `mbrgsfnativrddata()`, then stores the resulting record kind and error code into `mbioptr->newkind` and `mbioptr->newerror`.

`mbrgsfnativrddata()` reads the fixed 8-byte record header, optionally skips a checksum word, ensures the body buffer is large enough, reads the full body, and dispatches by record type.

### Header decode

For a `HEADER` record, `mbrgsfnativrdheader()` simply copies 12 bytes into the version field. This is one of the simplest examples in the module and a good place to get comfortable with pointer-based decoding.

### Ping decode

For a ping record, `mbrgsfnativrdping()` first decodes the fixed 56-byte ping header. It then walks through the rest of the body subrecord by subrecord using a loop and a moving byte index.

This style is very common in binary parsers. Instead of slicing bytes like in Python, the code advances an integer offset through a `char *` buffer and decodes values at each step.

### Array decode

`mbrgsfnativrdbeamarray()` is a reusable helper for scaled numeric beam arrays. It looks up the relevant scale-factor entry, determines the per-element field size, reads each raw value, and converts it into a decoded double.

This is a good example of replacing duplicated code with one generic function. In Python you might pass a decoder callback or use NumPy vectorization; in C, a compact loop plus explicit parameters is often the simplest solution.

### Attitude decode

`mbrgsfnativrdattitude()` decodes base time, sample count, time offsets, pitch, roll, heave, and heading. It also stores the base time as the record timestamp and pushes every sample into MB-System's interpolation buffer with `mbattintadd()`.

In the validated v4 code, the parameter order for `mbattintadd()` was confirmed against the actual MB-System prototype, so the guide no longer needs to treat that call as uncertain.

## Extraction functions

### Dimensions and ping number

`mbsysgsfnativedimensions()` reports the number of bathymetry and amplitude values available for the current record. For data pings, both counts are set from the ping beam count, while sidescan remains zero because this module does not decode sidescan.

`mbsysgsfnativepingnumber()` returns zero because the decoded subset does not include a sequential ping-number field. The validated signature uses `unsigned int` to match MB-System's actual function-pointer slot.

### Bathymetry extraction

`mbsysgsfnativeextract()` is the main function that MB-System uses to obtain standard outputs from the decoded record. For ping records, it returns time, navigation, heading, speed, bathymetry, amplitudes, across-track distances, along-track distances, and per-beam flags.

The whole ping is suppressed if the GSF ping-flag ignore bit is set. Otherwise, each beam is marked good or flagged depending on the beam's GSF beam-flag ignore bit.

### Beam flags

In v4, beam-flag combinations use bitwise OR, written as `MBFLAGFLAG | MBFLAGMANUAL`, rather than arithmetic addition. The numeric result is the same for these specific constants, but OR is the correct and clearer operation when combining independent bit flags.

If you are new to C bit flags, think of them as small binary masks where each bit has a meaning. OR turns on multiple named bits at once without depending on their numeric values adding cleanly.

### Navigation extraction

`mbsysgsfnativeextractnav()` returns navigation and attitude-like values from the ping header. Draft comes from `depthcorrector`, and roll, pitch, and heave come directly from the ping header rather than from interpolated attitude records.

That design is deliberate. The module does decode separate attitude records and stores them in MB-System's interpolation buffer, but this extraction function does not override the ping-header values automatically.

### Altitude extraction

`mbsysgsfnativeextractaltitude()` estimates altitude as the shallowest valid beam depth minus the transducer depth. If no valid beam exists, it returns failure with `MBERRORMISSINGDATA` in the validated v4 implementation.

This replaced a nonexistent error constant from the earlier draft. The change matters because using undefined constants is the kind of small issue that can stop C code from compiling cleanly.

### Travel times, detects, gains, and copy

`mbsysgsfnativettimes()` reports no travel-time beams because this module does not decode the GSF travel-time and beam-angle arrays. `mbsysgsfnativedetects()` reports unknown detection method, and `mbsysgsfnativegains()` reports zeros because the necessary source subrecords are outside v1 scope.

`mbsysgsfnativecopy()` performs a shallow struct copy, which is correct here because the main store struct contains no internal heap pointers of its own. In Python, plain assignment shares references, but in C a struct assignment copies the full flat value layout.

## Format metadata and registration outside the module

To make MB-System recognize the new format, you must also update `mbformat.h`, `mbformat.c`, and `Makefile.am`. The validation report confirms that these edits are required outside the three module files themselves.

- Add the new data-system define `MBSYSGSFNATIVE` and format define `MBFGSFNATIV` in `mbformat.h`, after checking that the chosen ids are still free in your checkout.
- Add the `mbrregistergsfnativ()` and `mbrinfogsfnativ()` prototypes in `mbformat.h`.
- Add the registration and format-info dispatch entries in `mbformat.c`.
- Add `mbsysgsfnative.h`, `mbsysgsfnative.c`, and `mbrgsfnativ.c` to the unconditional build lists in `Makefile.am`, not inside the `ENABLEGSF` conditional used by format 121.

## Three open GSF-spec questions

The MB-System-facing API details have been validated, but three GSF format details remain open because they can only be confirmed against the GSF specification PDF or real test files. The source files mark each one with `TODO-VERIFY-VS-SPEC` at the exact point of use.

1. The exact bit split of the ping-body subrecord header word, specifically how subrecord id and size are packed.
2. The exact bit split of the scale-factor `idandflags` word, including subrecord id, field-size selector, and compression flag.
3. The physical unit of the attitude per-sample time offset, currently assumed to be milliseconds.

### How to resolve them

The fastest path is to inspect the exact figures and tables in the GSF specification PDF referenced by the guide, then compare those definitions against one or more real GSF files using a hex dump. In practice, one small test file that contains one ping with known scale factors and one attitude record is usually enough to settle all three questions.

Useful ways you can assist are practical and concrete:

- Provide the current GSF specification PDF so the exact figures can be read directly.
- Provide one or two small sample GSF files, ideally with known contents and without confidentiality concerns.
- If available, provide decoded output from format 121 for the same files so the native reader can be compared field by field.
- If you already know the expected meaning of the attitude offset field from another implementation, provide that reference so it can be checked against the spec.

## Testing strategy

A strong validation strategy is to run the native reader and the existing format 121 reader on the same GSF files, then compare the extracted outputs record by record. Because both are intended to describe the same physical data, any mismatch points to a decode bug, an interpretation difference, or one of the remaining open GSF-spec questions.

For beginner-friendly debugging, start with the smallest possible file and use high MB-System verbosity so you can inspect values as they are decoded. In C binary parsing, reducing the amount of input often shortens debugging time much more than adding more logging alone.


## Appendix A: `mbsys_gsfnative.h`

```c
/*--------------------------------------------------------------------
 * mbsys_gsfnative.h  --  VALIDATED v4
 *
 * MB-System data-system header for a NATIVE (non-libgsf) reader of
 * the Generic Sensor Format (GSF), version 03.11.
 *
 * Registered as:
 *   Data system name : gsfnative      (MB_SYS_GSFNATIVE, suggested = 42)
 *   Format name      : GSFNATIV       (8 characters, suggested format id = 300)
 *
 * This module is independent of MB-System's existing GSF support
 * (format 121, MBF_GSFGENMB / mbsys_gsf.*), which wraps the external
 * Leidos libgsf C library. This module parses the documented GSF
 * byte stream directly.
 *
 * VALIDATION NOTE (this v4 revision): this header, its companion
 * mbsys_gsfnative.c and mbr_gsfnativ.c, were cross-checked line by
 * line against the following real MB-System 5.8.3beta16 source
 * files supplied by the user: mb_io.h, mb_define.h, mb_status.h,
 * mb_format.h, mb_format.c, mb_swap.c, mb_mem.c, mb_fileio.c,
 * mb_time.c, mbsys_templatesystem.h/.c, mbr_tempform.c,
 * mbsys_gsf.h/.c, mbr_gsfgenmb.c, and src/mbio/Makefile.am. Every
 * MBIO-side struct field, function-pointer slot, and helper-function
 * signature referenced below was verified against those files (not
 * assumed). Only bit-level packing that is defined by the GSF Spec
 * itself (not by MB-System) remains flagged TODO-VERIFY, since no
 * MB-System source file can confirm GSF's own on-disk bit layout.
 *
 * SCOPE (v1): read-only. Handles GSF record types HEADER (1),
 * SWATHBATHYMETRYPING (2), and ATTITUDE (12) with full decoding.
 * Every other GSF record type is read as an opaque, length-known
 * block and skipped, but is now correctly tagged with the exact
 * MB-System record kind that already exists for it (see the
 * dispatch table in mbr_gsfnativ.c) instead of the previously
 * invented and nonexistent MB_DATA_OTHER.
 *
 * Within the ping record, only these array subrecords are decoded:
 * DEPTHARRAY (1), ACROSSTRACKARRAY (2), ALONGTRACKARRAY (3),
 * MEANCALAMPLITUDEARRAY (6), BEAMFLAGSARRAY (16), and the
 * SCALEFACTORS (100) subrecord required to decode the numeric
 * arrays. All other subrecords are skipped by declared size.
 *
 * OPEN ITEMS still requiring confirmation against the GSF Spec PDF
 * itself (these are GSF-format questions, not MB-System-API
 * questions, so no amount of MB-System source review resolves them):
 *   1. Exact bit packing of the scale-factor entry's id/flags word
 *      (Spec Figure 4-7).
 *   2. Exact bit packing of the ping-body subrecord header word
 *      (Spec Figure 4-6).
 *   3. Physical unit of the Attitude record's ATTITUDETIME offset
 *      field (assumed milliseconds here).
 * Each is marked at its point of use with a "TODO-VERIFY-VS-SPEC"
 * comment. Items resolved in this revision by MB-System source
 * review are marked "RESOLVED v4" at their point of use.
 *------------------------------------------------------------------*/

#ifndef MBSYS_GSFNATIVE_H_
#define MBSYS_GSFNATIVE_H_

/*--------------------------------------------------------------------
 * Record and subrecord identifiers (GSF Spec Appendix A.1 / A.2)
 *------------------------------------------------------------------*/

/* GSF top-level record identifiers -- registry 0 */
#define MBSYS_GSFNTV_REC_HEADER                1
#define MBSYS_GSFNTV_REC_SWATHBATHYMETRYPING    2
#define MBSYS_GSFNTV_REC_SOUNDVELOCITYPROFILE    3
#define MBSYS_GSFNTV_REC_PROCESSINGPARAMETERS    4
#define MBSYS_GSFNTV_REC_SENSORPARAMETERS      5
#define MBSYS_GSFNTV_REC_COMMENT               6
#define MBSYS_GSFNTV_REC_HISTORY               7
#define MBSYS_GSFNTV_REC_NAVIGATIONERROR       8  /* obsolete since GSF v1.07 */
#define MBSYS_GSFNTV_REC_SWATHBATHYSUMMARY      9
#define MBSYS_GSFNTV_REC_SINGLEBEAMSOUNDING    10  /* discouraged since GSF v2.03 */
#define MBSYS_GSFNTV_REC_HVNAVIGATIONERROR     11
#define MBSYS_GSFNTV_REC_ATTITUDE              12

/* GSF swath bathymetry ping subrecord identifiers */
#define MBSYS_GSFNTV_SUB_DEPTHARRAY             1
#define MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY       2
#define MBSYS_GSFNTV_SUB_ALONGTRACKARRAY        3
#define MBSYS_GSFNTV_SUB_TRAVELTIMEARRAY        4
#define MBSYS_GSFNTV_SUB_BEAMANGLEARRAY         5
#define MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY  6
#define MBSYS_GSFNTV_SUB_MEANRELAMPLITUDEARRAY  7
#define MBSYS_GSFNTV_SUB_ECHOWIDTHARRAY         8
#define MBSYS_GSFNTV_SUB_QUALITYFACTORARRAY     9
#define MBSYS_GSFNTV_SUB_RECEIVEHEAVEARRAY     10
#define MBSYS_GSFNTV_SUB_NOMINALDEPTHARRAY     14
#define MBSYS_GSFNTV_SUB_QUALITYFLAGSARRAY     15  /* obsolete */
#define MBSYS_GSFNTV_SUB_BEAMFLAGSARRAY        16
#define MBSYS_GSFNTV_SUB_SIGNALTONOISEARRAY    17
#define MBSYS_GSFNTV_SUB_BEAMANGLEFORWARDARRAY 18
#define MBSYS_GSFNTV_SUB_VERTICALERRORARRAY    19
#define MBSYS_GSFNTV_SUB_HORIZONTALERRORARRAY  20
#define MBSYS_GSFNTV_SUB_SCALEFACTORS          100

/* Top-level record identifier word bit masks (Spec Sec 4.3.1.1/4.3.1.2).
 * RESOLVED v4: removed the previous pre-shifted MBSYS_GSFNTV_REGISTRY_MASK
 * constant, which was numerically wrong (0x000FFC00u) and was, in any case,
 * dead code never referenced by the actual header-parsing logic. Registry
 * extraction (unused by this v1 module but provided for completeness) is
 * now expressed the same shift-then-mask way as the already-correct
 * data-type extraction below. */
#define MBSYS_GSFNTV_CHECKSUM_FLAG_MASK  0x80000000u  /* bit 31 */
#define MBSYS_GSFNTV_DATATYPE_MASK       0x00000FFFu  /* bits 11-0 */
#define MBSYS_GSFNTV_REGISTRY_SHIFT      12
#define MBSYS_GSFNTV_REGISTRY_MASK       0x000003FFu  /* apply AFTER >> 12 */

/* Ping flag bit (Spec Appendix C.1) */
#define MBSYS_GSFNTV_PINGFLAG_IGNORE 0x0001

/* Beam flag bits (Spec Appendix C.2) */
#define MBSYS_GSFNTV_BEAMFLAG_IGNORE   0x01
#define MBSYS_GSFNTV_BEAMFLAG_SELECTED 0x02

/*--------------------------------------------------------------------
 * Array size limits
 *------------------------------------------------------------------*/
#define MBSYS_GSFNTV_MAX_BEAMS        1024
#define MBSYS_GSFNTV_MAX_SCALEFACTORS   32
#define MBSYS_GSFNTV_MAX_ATTITUDE     1024
#define MBSYS_GSFNTV_VERSION_SIZE       12

/*--------------------------------------------------------------------
 * Header Record -- 12 bytes, one text field
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_header_struct {
    char version[MBSYS_GSFNTV_VERSION_SIZE]; /* e.g. "GSF-v03.11", not guaranteed null-terminated on disk */
};

/*--------------------------------------------------------------------
 * Swath Bathymetry Ping Record -- Ping Header (56 bytes, GSF v03.01+)
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_pingheader_struct {
    int            time_sec;           /* POSIX seconds */
    int            time_nsec;          /* nanoseconds within the second */
    int            longitude_raw;      /* 1e-7 degree, signed, +East */
    int            latitude_raw;       /* 1e-7 degree, signed, +North */
    short          num_beams;          /* N */
    short          center_beam;        /* column index nearest keel */
    unsigned short ping_flags;
    unsigned short reserved;
    short          tide_corrector;     /* centimeters, already applied to depths */
    int            depth_corrector;    /* centimeters, already applied to depths */
    unsigned short heading_raw;        /* hundredths of a degree, 0-36000 */
    short          pitch_raw;          /* hundredths of a degree, signed */
    short          roll_raw;           /* hundredths of a degree, signed */
    short          heave;              /* centimeters, signed */
    unsigned short course_raw;         /* hundredths of a degree */
    unsigned short speed_raw;          /* hundredths of a knot */
    int            height;             /* 0.001 m, GSF v03.01+ only */
    int            separation;         /* 0.001 m, GSF v03.01+ only */
    int            gps_tide_corrector; /* millimeters, GSF v03.01+ only */
    short          spare;
};

/*--------------------------------------------------------------------
 * Scale-factor subrecord
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_scalefactor_entry_struct {
    int   subrecord_id;    /* which array subrecord this scale factor applies to */
    int   field_size;      /* decoded element width in bytes: 1, 2, or 4 */
    int   compression_flag;
    float multiplier;
    float offset;
};

struct mbsys_gsfntv_scalefactors_struct {
    int num_factors; /* M */
    struct mbsys_gsfntv_scalefactor_entry_struct factor[MBSYS_GSFNTV_MAX_SCALEFACTORS];
};

/*--------------------------------------------------------------------
 * Swath Bathymetry Ping Record -- decoded ping
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_ping_struct {
    struct mbsys_gsfntv_pingheader_struct   header;
    struct mbsys_gsfntv_scalefactors_struct scalefactors; /* persists until a new instance appears */

    double navlon;   /* decimal degrees */
    double navlat;   /* decimal degrees */
    double heading;  /* decimal degrees */
    double speed;    /* knots */

    double        depth[MBSYS_GSFNTV_MAX_BEAMS];        /* meters */
    double        across_track[MBSYS_GSFNTV_MAX_BEAMS]; /* meters */
    double        along_track[MBSYS_GSFNTV_MAX_BEAMS];  /* meters */
    double        amplitude[MBSYS_GSFNTV_MAX_BEAMS];    /* dB */
    unsigned char beam_flags[MBSYS_GSFNTV_MAX_BEAMS];   /* raw 8-bit GSF beam flag */
};

/*--------------------------------------------------------------------
 * Attitude record
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_attitude_struct {
    int    base_time_sec;
    int    base_time_nsec;
    double base_time_d;
    int    num_measurements; /* N, per record: <= 60 sec worth */
    double sample_time_d[MBSYS_GSFNTV_MAX_ATTITUDE];
    double pitch[MBSYS_GSFNTV_MAX_ATTITUDE];
    double roll[MBSYS_GSFNTV_MAX_ATTITUDE];
    double heave[MBSYS_GSFNTV_MAX_ATTITUDE];
    double heading[MBSYS_GSFNTV_MAX_ATTITUDE];
};

/*--------------------------------------------------------------------
 * Primary data-system structure
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_struct {
    int    kind; /* MB_DATA_DATA, MB_DATA_ATTITUDE, MB_DATA_HEADER, MB_DATA_NONE,
                    or one of the pre-existing GSF-specific kinds already defined
                    in mb_status.h for skipped record types -- see mbr_gsfnativ.c */
    double time_d;
    int    time_i[7];

    struct mbsys_gsfntv_header_struct    header;
    struct mbsys_gsfntv_ping_struct      ping;
    struct mbsys_gsfntv_attitude_struct  attitude;
};

/*--------------------------------------------------------------------
 * Function prototypes -- mbsys_gsfnative.c
 * VALIDATED against struct mb_io_struct's function-pointer slots
 * in mb_io.h. Fixes applied in this revision:
 *   - mbsys_gsfnative_pingnumber(): pingnumber is unsigned int*,
 *     matching mb_io_ptr->mb_io_pingnumber exactly (was int* before).
 *------------------------------------------------------------------*/

int mbsys_gsfnative_alloc(int verbose, void *mbio_ptr, void **store_ptr, int *error);

int mbsys_gsfnative_deall(int verbose, void *mbio_ptr, void **store_ptr, int *error);

int mbsys_gsfnative_dimensions(int verbose, void *mbio_ptr, void *store_ptr,
                                int *kind, int *nbath, int *namp, int *nss, int *error);

int mbsys_gsfnative_pingnumber(int verbose, void *mbio_ptr, unsigned int *pingnumber, int *error);

int mbsys_gsfnative_sonartype(int verbose, void *mbio_ptr, void *store_ptr,
                               int *sonartype, int *error);

int mbsys_gsfnative_extract(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int time_i[7], double *time_d,
                             double *navlon, double *navlat, double *speed, double *heading,
                             int *nbath, int *namp, int *nss,
                             char *beamflag, double *bath, double *amp,
                             double *bathacrosstrack, double *bathalongtrack,
                             double *ss, double *ssacrosstrack, double *ssalongtrack,
                             char *comment, int *error);

int mbsys_gsfnative_insert(int verbose, void *mbio_ptr, void *store_ptr,
                            int kind, int time_i[7], double time_d,
                            double navlon, double navlat, double speed, double heading,
                            int nbath, int namp, int nss,
                            char *beamflag, double *bath, double *amp,
                            double *bathacrosstrack, double *bathalongtrack,
                            double *ss, double *ssacrosstrack, double *ssalongtrack,
                            char *comment, int *error);

int mbsys_gsfnative_extract_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                 int *kind, int time_i[7], double *time_d,
                                 double *navlon, double *navlat, double *speed, double *heading,
                                 double *draft, double *roll, double *pitch, double *heave, int *error);

int mbsys_gsfnative_insert_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                int time_i[7], double time_d,
                                double navlon, double navlat, double speed, double heading,
                                double draft, double roll, double pitch, double heave, int *error);

int mbsys_gsfnative_extract_altitude(int verbose, void *mbio_ptr, void *store_ptr,
                                      int *kind, double *transducer_depth, double *altitude, int *error);

int mbsys_gsfnative_ttimes(int verbose, void *mbio_ptr, void *store_ptr,
                            int *kind, int *nbeams,
                            double *ttime, double *angle_xtrack, double *angle_ltrack,
                            double *heave, double *alongtrack_offset,
                            double *draft, double *ssv, int *error);

int mbsys_gsfnative_detects(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int *nbeams, int *detects, int *error);

int mbsys_gsfnative_gains(int verbose, void *mbio_ptr, void *store_ptr,
                           int *kind, double *transmit_gain, double *pulse_length,
                           double *receive_gain, int *error);

int mbsys_gsfnative_copy(int verbose, void *mbio_ptr,
                          void *store_ptr, void *copy_ptr, int *error);

/*--------------------------------------------------------------------
 * Function prototypes -- mbr_gsfnativ.c
 * VALIDATED against mbr_tempform.c / mb_format.h. Fix applied:
 *   - mbr_info_gsfnativ(): added platform_source, sensordepth_source;
 *     renamed vru_source -> attitude_source; variable_beams/
 *     traveltime/beam_flagging changed from int* to bool*, matching
 *     the real signature used by EVERY mbr_info_XXX() in mb_format.h.
 *------------------------------------------------------------------*/

int mbr_register_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_info_gsfnativ(int verbose,
                       int *system, int *beams_bath_max, int *beams_amp_max, int *pixels_ss_max,
                       char *format_name, char *system_name, char *format_description,
                       int *numfile, int *filetype,
                       bool *variable_beams, bool *traveltime, bool *beam_flagging,
                       int *platform_source, int *nav_source, int *sensordepth_source,
                       int *heading_source, int *attitude_source, int *svp_source,
                       double *beamwidth_xtrack, double *beamwidth_ltrack, int *error);

int mbr_alm_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_dem_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_rt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error);

int mbr_wt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error);

/* internal helper functions, used only within mbr_gsfnativ.c */
int mbr_gsfnativ_rd_data(int verbose, void *mbio_ptr, void *store_ptr, int *error);
int mbr_gsfnativ_rd_header(int verbose, char *buf, struct mbsys_gsfntv_struct *store, int *error);
int mbr_gsfnativ_rd_ping(int verbose, void *mbio_ptr, char *buf, int body_len,
                          struct mbsys_gsfntv_struct *store, int *error);
int mbr_gsfnativ_rd_scalefactors(int verbose, void *mbio_ptr, char *buf,
                                  struct mbsys_gsfntv_scalefactors_struct *sf, int *error);
int mbr_gsfnativ_rd_beamarray(int verbose, void *mbio_ptr, char *buf, int num_beams,
                               struct mbsys_gsfntv_scalefactors_struct *sf, int target_subrecord_id,
                               double *out_array, int *error);
int mbr_gsfnativ_rd_attitude(int verbose, void *mbio_ptr, char *buf,
                              struct mbsys_gsfntv_struct *store, int *error);

double mbsys_gsfnative_decode_scaled(int raw_value, float multiplier, float offset);
int    mbsys_gsfnative_encode_scaled(double real_value, float multiplier, float offset);

#endif /* MBSYS_GSFNATIVE_H_ */
```


## Appendix B: `mbsys_gsfnative.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mb_status.h"
#include "mb_format.h"
#include "mb_define.h"
#include "mb_io.h"
#include "mbsys_gsfnative.h"

static const char rcs_id[] = "$Id: mbsys_gsfnative.c, native GSF v03.11 reader, validated v4 $";

/*--------------------------------------------------------------------*/
/* Scale-factor decode/encode helpers */
/*--------------------------------------------------------------------*/

double mbsys_gsfnative_decode_scaled(int raw_value, float multiplier, float offset) {
    /* GSFlib Docs: the offset exists specifically to let negative
     * real-world values be represented by an UNSIGNED stored integer,
     * which is only consistent with subtracting the offset AFTER
     * dividing by the multiplier. */
    return ((double)raw_value / (double)multiplier) - (double)offset;
}

int mbsys_gsfnative_encode_scaled(double real_value, float multiplier, float offset) {
    return (int)((real_value + (double)offset) * (double)multiplier);
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_alloc(int verbose, void *mbio_ptr, void **store_ptr, int *error) {
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:  %d\n", verbose);
        fprintf(stderr, "dbg2  mbio_ptr: %p\n", (void *)mbio_ptr);
    }

    status = mb_mallocd(verbose, __FILE__, __LINE__,
                         sizeof(struct mbsys_gsfntv_struct), (void **)store_ptr, error);

    if (status == MB_SUCCESS)
        memset(*store_ptr, 0, sizeof(struct mbsys_gsfntv_struct));

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  store_ptr: %p\n", (void *)*store_ptr);
        fprintf(stderr, "dbg2  error:     %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status:    %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_deall(int verbose, void *mbio_ptr, void **store_ptr, int *error) {
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:   %d\n", verbose);
        fprintf(stderr, "dbg2  store_ptr: %p\n", (void *)*store_ptr);
    }

    status = mb_freed(verbose, __FILE__, __LINE__, (void **)store_ptr, error);

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_dimensions(int verbose, void *mbio_ptr, void *store_ptr,
                                int *kind, int *nbath, int *namp, int *nss, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    int status = MB_SUCCESS;

    *kind = store->kind;
    if (*kind == MB_DATA_DATA) {
        *nbath = store->ping.header.num_beams;
        *namp = *nbath; /* GSF mean-calibrated-amplitude array shares the depth-array beam indexing */
        *nss = 0;        /* no sidescan in this module */
    } else {
        *nbath = 0;
        *namp = 0;
        *nss = 0;
    }

    *error = MB_ERROR_NO_ERROR;
    return status;
}

/*--------------------------------------------------------------------*/
/* FIX 1: signature changed to unsigned int *pingnumber to match
 * struct mb_io_struct's mb_io_pingnumber slot exactly (mb_io.h). */
int mbsys_gsfnative_pingnumber(int verbose, void *mbio_ptr, unsigned int *pingnumber, int *error) {
    /* GSF's ping header does not carry an explicit sequential ping-number
     * field in the subset decoded by this module; left at zero. A real
     * ping-number field, if needed, would come from a Processing
     * Parameters or Sensor Parameters record, outside this module's
     * v1 scope. */
    *pingnumber = 0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_sonartype(int verbose, void *mbio_ptr, void *store_ptr,
                               int *sonartype, int *error) {
    /* Sensor identity is not decoded by this module's v1 scope (it would
     * come from a sensor-specific ping subrecord or a Processing/Sensor
     * Parameter record). Report "unknown" rather than guessing.
     * CONFIRMED: MB_TOPOGRAPHY_TYPE_UNKNOWN (value 0) exists in mb_status.h. */
    *sonartype = MB_TOPOGRAPHY_TYPE_UNKNOWN;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_extract(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int time_i[7], double *time_d,
                             double *navlon, double *navlat, double *speed, double *heading,
                             int *nbath, int *namp, int *nss,
                             char *beamflag, double *bath, double *amp,
                             double *bathacrosstrack, double *bathalongtrack,
                             double *ss, double *ssacrosstrack, double *ssalongtrack,
                             char *comment, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
    }

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;
    *nss = 0;

    if (*kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = ping->navlon;
        *navlat = ping->navlat;
        *heading = ping->heading;
        *speed = ping->speed * 1.852; /* knots -> km/hr, MB-System standard speed unit */

        /* Spec Appendix C.1: bit 0 of PINGFLAGS set -> whole ping unusable */
        if (ping->header.ping_flags & MBSYS_GSFNTV_PINGFLAG_IGNORE) {
            *nbath = 0;
            *namp = 0;
            if (verbose >= 2)
                fprintf(stderr, "dbg2  Ping flagged entirely unusable (PINGFLAGS bit 0 set)\n");
        } else {
            *nbath = ping->header.num_beams;
            *namp = *nbath;

            if (*nbath > MBSYS_GSFNTV_MAX_BEAMS)
                *nbath = MBSYS_GSFNTV_MAX_BEAMS; /* defensive clamp; should never trigger */

            for (i = 0; i < *nbath; i++) {
                bath[i] = ping->depth[i];
                bathacrosstrack[i] = ping->across_track[i];
                bathalongtrack[i] = ping->along_track[i];
                amp[i] = ping->amplitude[i];

                /* Spec Appendix C.2: bit 0 of the per-beam flag = ignore/invalid.
                 * FIX 3: OR instead of + for combining independent bit flags. */
                if (ping->beam_flags[i] & MBSYS_GSFNTV_BEAMFLAG_IGNORE)
                    beamflag[i] = MB_FLAG_FLAG | MB_FLAG_MANUAL;
                else
                    beamflag[i] = MB_FLAG_NONE;
            }
        }
    } else if (*kind == MB_DATA_COMMENT) {
        /* Comment record (GSF record id 6) is outside this module's v1 scope. */
        if (comment != NULL)
            comment[0] = '\0';
        *nbath = 0;
        *namp = 0;
    } else {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *nbath = 0;
        *namp = 0;
    }

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  kind:   %d\n", *kind);
        fprintf(stderr, "dbg2  nbath:  %d\n", *nbath);
        fprintf(stderr, "dbg2  error:  %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_insert(int verbose, void *mbio_ptr, void *store_ptr,
                            int kind, int time_i[7], double time_d,
                            double navlon, double navlat, double speed, double heading,
                            int nbath, int namp, int nss,
                            char *beamflag, double *bath, double *amp,
                            double *bathacrosstrack, double *bathalongtrack,
                            double *ss, double *ssacrosstrack, double *ssalongtrack,
                            char *comment, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    store->kind = kind;

    if (kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) store->time_i[i] = time_i[i];
        store->time_d = time_d;

        ping->navlon = navlon;
        ping->navlat = navlat;
        ping->heading = heading;
        ping->speed = speed / 1.852; /* km/hr -> knots, reverse of extract() */

        ping->header.num_beams = (short)(nbath < MBSYS_GSFNTV_MAX_BEAMS ? nbath : MBSYS_GSFNTV_MAX_BEAMS);

        for (i = 0; i < ping->header.num_beams; i++) {
            ping->depth[i] = bath[i];
            ping->across_track[i] = bathacrosstrack[i];
            ping->along_track[i] = bathalongtrack[i];
            ping->amplitude[i] = (i < namp) ? amp[i] : 0.0;

            if (beamflag[i] == MB_FLAG_NONE)
                ping->beam_flags[i] = 0;
            else
                ping->beam_flags[i] = MBSYS_GSFNTV_BEAMFLAG_IGNORE;
        }
    } else if (kind == MB_DATA_COMMENT) {
        /* comment storage intentionally omitted -- out of v1 scope */
    }

    *error = MB_ERROR_NO_ERROR;
    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_extract_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                 int *kind, int time_i[7], double *time_d,
                                 double *navlon, double *navlat, double *speed, double *heading,
                                 double *draft, double *roll, double *pitch, double *heave, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;

    if (*kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = ping->navlon;
        *navlat = ping->navlat;
        *heading = ping->heading;
        *speed = ping->speed * 1.852;
        *draft = (double)ping->header.depth_corrector / 100.0; /* centimeters -> meters */

        /* Ping header's own roll/pitch/heave. If the calling program has
         * already accumulated Attitude-record samples via mb_attint_add()
         * and prefers the independently time-stamped, potentially
         * higher-rate series, it should call mb_attint_interp() itself
         * rather than rely on these ping-header values -- this function
         * intentionally does not override them here. */
        *roll = (double)ping->header.roll_raw / 100.0;
        *pitch = (double)ping->header.pitch_raw / 100.0;
        *heave = (double)ping->header.heave / 100.0;
    } else {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = 0.0;
        *navlat = 0.0;
        *speed = 0.0;
        *heading = 0.0;
        *draft = 0.0;
        *roll = 0.0;
        *pitch = 0.0;
        *heave = 0.0;
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_insert_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                int time_i[7], double time_d,
                                double navlon, double navlat, double speed, double heading,
                                double draft, double roll, double pitch, double heave, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int i;

    for (i = 0; i < 7; i++) store->time_i[i] = time_i[i];
    store->time_d = time_d;

    ping->navlon = navlon;
    ping->navlat = navlat;
    ping->heading = heading;
    ping->speed = speed / 1.852;

    ping->header.depth_corrector = (int)(draft * 100.0);
    ping->header.roll_raw = (short)(roll * 100.0);
    ping->header.pitch_raw = (short)(pitch * 100.0);
    ping->header.heave = (short)(heave * 100.0);

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* FIX 2: replaced nonexistent MB_ERROR_VALUE_OUT_OF_RANGE with the
 * real, confirmed-to-exist MB_ERROR_MISSING_DATA (mb_status.h). */
int mbsys_gsfnative_extract_altitude(int verbose, void *mbio_ptr, void *store_ptr,
                                      int *kind, double *transducer_depth, double *altitude, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;
    double shallowest = 0.0;
    int found_valid = 0;

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;

    if (*kind == MB_DATA_DATA) {
        *transducer_depth = (double)ping->header.depth_corrector / 100.0;

        /* Altitude above bottom approximated as the shallowest usable
         * beam's depth minus the transducer depth. A production-quality
         * implementation would instead prefer the nadir (center) beam
         * specifically (ping->header.center_beam) when it is usable;
         * this simpler approach is adequate for a v1 module. */
        for (i = 0; i < ping->header.num_beams; i++) {
            if (!(ping->beam_flags[i] & MBSYS_GSFNTV_BEAMFLAG_IGNORE)) {
                if (!found_valid || ping->depth[i] < shallowest) {
                    shallowest = ping->depth[i];
                    found_valid = 1;
                }
            }
        }

        if (found_valid) {
            *altitude = shallowest - *transducer_depth;
        } else {
            *altitude = 0.0;
            *error = MB_ERROR_MISSING_DATA;
            status = MB_FAILURE;
        }
    } else {
        *transducer_depth = 0.0;
        *altitude = 0.0;
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_ttimes(int verbose, void *mbio_ptr, void *store_ptr,
                            int *kind, int *nbeams,
                            double *ttime, double *angle_xtrack, double *angle_ltrack,
                            double *heave, double *alongtrack_offset,
                            double *draft, double *ssv, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;

    /* This module's v1 scope does not decode TRAVELTIMEARRAY (subrecord id 4)
     * or BEAMANGLEARRAY (subrecord id 5) because it stores pre-computed
     * DEPTHARRAY/ACROSSTRACKARRAY/ALONGTRACKARRAY values directly
     * (mbr_info_gsfnativ() sets *traveltime = false accordingly).
     * Raytraced-bathymetry recalculation is therefore not supported;
     * this function reports zero beams rather than fabricating values. */
    *kind = store->kind;
    *nbeams = 0;
    *draft = (double)store->ping.header.depth_corrector / 100.0;
    *ssv = 0.0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_detects(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int *nbeams, int *detects, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int i;

    /* Bottom-detection method (amplitude vs. phase) is carried in GSF's
     * DETECTIONINFOARRAY subrecord (id 23), outside this module's v1
     * scope. Report as unknown for every beam rather than guessing. */
    *kind = store->kind;
    if (*kind == MB_DATA_DATA) {
        *nbeams = ping->header.num_beams;
        for (i = 0; i < *nbeams && i < MBSYS_GSFNTV_MAX_BEAMS; i++)
            detects[i] = MB_DETECT_UNKNOWN;
    } else {
        *nbeams = 0;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_gains(int verbose, void *mbio_ptr, void *store_ptr,
                           int *kind, double *transmit_gain, double *pulse_length,
                           double *receive_gain, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;

    /* Transmit/receive gain and pulse length are carried in sensor-specific
     * ping subrecords, outside this module's v1 scope. Report zero/unknown
     * rather than guessing. */
    *kind = store->kind;
    *transmit_gain = 0.0;
    *pulse_length = 0.0;
    *receive_gain = 0.0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_copy(int verbose, void *mbio_ptr,
                          void *store_ptr, void *copy_ptr, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_struct *copy = (struct mbsys_gsfntv_struct *)copy_ptr;

    *copy = *store; /* flat struct, no internal pointers -- a shallow copy is exact and sufficient */

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}
```


## Appendix C: `mbr_gsfnativ.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mb_status.h"
#include "mb_format.h"
#include "mb_define.h"
#include "mb_io.h"
#include "mbsys_gsfnative.h"

static const char rcs_id[] = "$Id: mbr_gsfnativ.c, native GSF v03.11 reader, validated v4 $";

/*--------------------------------------------------------------------*/
/* mbr_register_gsfnativ()
 * Loads function pointers into mb_io_struct so that generic MBIO
 * calls (mb_read_ping, mb_extract, mb_extract_nav, etc.) dispatch
 * to this module's functions when format 300 is in use. */
/*--------------------------------------------------------------------*/
int mbr_register_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:  %d\n", verbose);
        fprintf(stderr, "dbg2  mbio_ptr: %p\n", (void *)mbio_ptr);
    }

    /* FIX 1/2: full parameter list, matching the real mbr_info_XXX()
     * signature confirmed in mb_format.h (platform_source and
     * sensordepth_source added; attitude_source is the real field name). */
    status = mbr_info_gsfnativ(verbose,
                                &mb_io_ptr->system,
                                &mb_io_ptr->beams_bath_max,
                                &mb_io_ptr->beams_amp_max,
                                &mb_io_ptr->pixels_ss_max,
                                mb_io_ptr->format_name,
                                mb_io_ptr->system_name,
                                mb_io_ptr->format_description,
                                &mb_io_ptr->numfile,
                                &mb_io_ptr->filetype,
                                &mb_io_ptr->variable_beams,
                                &mb_io_ptr->traveltime,
                                &mb_io_ptr->beam_flagging,
                                &mb_io_ptr->platform_source,
                                &mb_io_ptr->nav_source,
                                &mb_io_ptr->sensordepth_source,
                                &mb_io_ptr->heading_source,
                                &mb_io_ptr->attitude_source,
                                &mb_io_ptr->svp_source,
                                &mb_io_ptr->beamwidth_xtrack,
                                &mb_io_ptr->beamwidth_ltrack,
                                error);

    /* set format and system specific function pointers -- only functions
     * that are implemented for this data system are registered; everything
     * else is left NULL so MBIO knows those capabilities are unavailable,
     * following the same convention used throughout MB-System's other
     * I/O modules (confirmed against mbr_tempform.c's mbr_register_tempform()). */
    mb_io_ptr->mb_io_format_alloc = &mbr_alm_gsfnativ;
    mb_io_ptr->mb_io_format_free = &mbr_dem_gsfnativ;
    mb_io_ptr->mb_io_store_alloc = &mbsys_gsfnative_alloc;
    mb_io_ptr->mb_io_store_free = &mbsys_gsfnative_deall;
    mb_io_ptr->mb_io_read_ping = &mbr_rt_gsfnativ;
    mb_io_ptr->mb_io_write_ping = &mbr_wt_gsfnativ;
    mb_io_ptr->mb_io_dimensions = &mbsys_gsfnative_dimensions;
    mb_io_ptr->mb_io_pingnumber = &mbsys_gsfnative_pingnumber;
    mb_io_ptr->mb_io_sonartype = &mbsys_gsfnative_sonartype;
    mb_io_ptr->mb_io_sidescantype = NULL; /* no sidescan support in this module */
    mb_io_ptr->mb_io_extract = &mbsys_gsfnative_extract;
    mb_io_ptr->mb_io_insert = &mbsys_gsfnative_insert;
    mb_io_ptr->mb_io_extract_nav = &mbsys_gsfnative_extract_nav;
    mb_io_ptr->mb_io_extract_nnav = NULL;
    mb_io_ptr->mb_io_insert_nav = &mbsys_gsfnative_insert_nav;
    mb_io_ptr->mb_io_extract_altitude = &mbsys_gsfnative_extract_altitude;
    mb_io_ptr->mb_io_insert_altitude = NULL;
    mb_io_ptr->mb_io_extract_svp = NULL; /* Sound Velocity Profile record not in v1 scope */
    mb_io_ptr->mb_io_insert_svp = NULL;
    mb_io_ptr->mb_io_ttimes = &mbsys_gsfnative_ttimes;
    mb_io_ptr->mb_io_detects = &mbsys_gsfnative_detects;
    mb_io_ptr->mb_io_gains = &mbsys_gsfnative_gains;
    mb_io_ptr->mb_io_copyrecord = &mbsys_gsfnative_copy;
    mb_io_ptr->mb_io_extract_rawss = NULL;
    mb_io_ptr->mb_io_insert_rawss = NULL;
    mb_io_ptr->mb_io_extract_segytraceheader = NULL;
    mb_io_ptr->mb_io_extract_segy = NULL;
    mb_io_ptr->mb_io_insert_segy = NULL;
    mb_io_ptr->mb_io_ctd = NULL;
    mb_io_ptr->mb_io_ancilliarysensor = NULL;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  system: %d\n", mb_io_ptr->system);
        fprintf(stderr, "dbg2  error:  %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_info_gsfnativ()
 * Sets format parameters and modes.
 * FIX 1: signature now exactly matches every real mbr_info_XXX() in
 * mb_format.h -- added platform_source/sensordepth_source parameters,
 * bool* for variable_beams/traveltime/beam_flagging, attitude_source
 * instead of vru_source. */
/*--------------------------------------------------------------------*/
int mbr_info_gsfnativ(int verbose,
                       int *system, int *beams_bath_max, int *beams_amp_max, int *pixels_ss_max,
                       char *format_name, char *system_name, char *format_description,
                       int *numfile, int *filetype,
                       bool *variable_beams, bool *traveltime, bool *beam_flagging,
                       int *platform_source, int *nav_source, int *sensordepth_source,
                       int *heading_source, int *attitude_source, int *svp_source,
                       double *beamwidth_xtrack, double *beamwidth_ltrack, int *error) {
    int status = MB_SUCCESS;

    *error = MB_ERROR_NO_ERROR;

    /* MB_SYS_GSFNATIVE must be defined in mb_format.h. Based on the
     * mb_format.h snapshot reviewed for this validation, the highest
     * assigned MB_SYS_* value is MB_SYS_RESON7K3 = 41, so 42 is free;
     * re-check this against your exact checkout before building, since
     * new data systems may have been added since this snapshot. */
    *system = MB_SYS_GSFNATIVE;
    *beams_bath_max = MBSYS_GSFNTV_MAX_BEAMS;
    *beams_amp_max = MBSYS_GSFNTV_MAX_BEAMS;
    *pixels_ss_max = 0; /* no sidescan support in this module */

    strncpy(format_name, "GSFNATIV", MB_NAME_LENGTH);
    strncpy(system_name, "gsfnative", MB_NAME_LENGTH);
    strncpy(format_description,
            "Format name: MBF_GSFNATIV\n"
            "Informal Description: Native (non-libgsf) reader for the Generic Sensor\n"
            "  Format (GSF), version 03.11.\n"
            "Attributes: Multibeam bathymetry and amplitude, variable beams,\n"
            "  big-endian binary, records HEADER/SWATHBATHYMETRYPING/\n"
            "  ATTITUDE decoded, all other GSF records skipped by size\n"
            "  (see mbsys_gsfnative.h for full scope).\n",
            MB_DESCRIPTION_LENGTH);

    *numfile = 1;
    *filetype = MB_FILETYPE_SINGLE; /* NOT MB_FILETYPE_GSF -- confirmed in mb_define.h */
    *variable_beams = true;
    *traveltime = false;  /* stores pre-computed depth, not raw travel time */
    *beam_flagging = true;
    *platform_source = MB_DATA_NONE;     /* no sensor-offset record decoded in v1 scope */
    *nav_source = MB_DATA_DATA;          /* position carried directly in the ping header */
    *sensordepth_source = MB_DATA_DATA;  /* ping header depth_corrector/height fields */
    *heading_source = MB_DATA_DATA;
    *attitude_source = MB_DATA_DATA;     /* ping header carries roll/pitch/heave directly;
                                             MB_DATA_ATTITUDE records are also decoded and fed
                                             to mb_attint_add() for interpolation if preferred */
    *svp_source = MB_DATA_NONE;          /* Sound Velocity Profile record not in v1 scope */
    *beamwidth_xtrack = 0.0;             /* GSF's ping/attitude records carry no fixed
                                             system beamwidth; left at 0.0 (unknown) */
    *beamwidth_ltrack = 0.0;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbr_alm_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    /* allocate the persistent record-body read buffer, tracked via the
     * generic mb_io_struct scratch fields (confirmed to exist exactly as
     * named in mb_io.h: void *saveptr1; int save6;), and the data-system
     * storage structure itself */
    mb_io_ptr->saveptr1 = NULL;
    mb_io_ptr->save6 = 0; /* current allocated size of the buffer at saveptr1 */

    /* FIX 3: GSF is always big-endian on disk. mb_swap_check() (mb_swap.c)
     * returns true exactly when the host is NOT big-endian -- i.e. exactly
     * when byte-swapping is required to interpret GSF's bytes correctly on
     * this host. Cache that once here rather than re-deriving it per call. */
    mb_io_ptr->byteswapped = mb_swap_check();

    mb_io_ptr->structure_size = 0;
    mb_io_ptr->data_structure_size = sizeof(struct mbsys_gsfntv_struct);

    status = mbsys_gsfnative_alloc(verbose, mbio_ptr, &mb_io_ptr->store_data, error);

    return status;
}

/*--------------------------------------------------------------------*/
int mbr_dem_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    if (mb_io_ptr->saveptr1 != NULL)
        status = mb_freed(verbose, __FILE__, __LINE__, (void **)&mb_io_ptr->saveptr1, error);
    mb_io_ptr->save6 = 0;

    status = mbsys_gsfnative_deall(verbose, mbio_ptr, &mb_io_ptr->store_data, error);

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_rt_gsfnativ() -- top-level per-call read entry point required by MBIO. */
/*--------------------------------------------------------------------*/
int mbr_rt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    status = mbr_gsfnativ_rd_data(verbose, mbio_ptr, store_ptr, error);

    mb_io_ptr->new_error = *error;
    mb_io_ptr->new_kind = ((struct mbsys_gsfntv_struct *)store_ptr)->kind;

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_wt_gsfnativ() -- write entry point. NOT YET IMPLEMENTED: this v1
 * module is read-only. Encoding a GSF record back to bytes requires the
 * inverse of every decode step in mbr_gsfnativ_rd_ping()/_rd_attitude()/
 * _rd_header(), plus re-deriving or re-using scale factors and
 * recomputing the 8-byte record header's size field -- all
 * straightforward given the decode logic already written, but
 * deliberately deferred so this first version can be validated against
 * format 121's known-correct output before any write path risks
 * corrupting a file. MB_ERROR_WRITE_FAIL is confirmed to exist in
 * mb_status.h (value 5). */
/*--------------------------------------------------------------------*/
int mbr_wt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    *error = MB_ERROR_WRITE_FAIL;
    fprintf(stderr, "mbr_wt_gsfnativ: write support not implemented in this v1 "
                     "(read-only) native GSF module.\n");
    return MB_FAILURE;
}

/*--------------------------------------------------------------------*/
/* mbr_gsfnativ_rd_data()
 * Reads the 8- or 12-byte record header, reads the full record body
 * into a persistent growable buffer, and dispatches on record type.
 *
 * FIX 4: the previous default case tagged every skipped record with
 * the nonexistent MB_DATA_OTHER. This revision maps each GSF record
 * type to the real, pre-existing MB-System kind already defined for
 * it in mb_status.h. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_data(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    char header_buf[8];
    char checksum_buf[4];
    unsigned int record_size;
    unsigned int id_word;
    unsigned int body_bytes_remaining;
    int record_type;
    bool checksum_present;
    char *body_buf;
    size_t read_len;
    int status;

    /* --- 1. read the fixed 8-byte size+identifier header --- */
    read_len = (size_t)8;
    status = mb_fileio_get(verbose, mbio_ptr, header_buf, &read_len, error);
    if (status != MB_SUCCESS) {
        store->kind = MB_DATA_NONE;
        return status; /* MB_ERROR_EOF at genuine end of file, or a read failure */
    }

    /* FIX 3: use mb_io_ptr->byteswapped instead of a hardcoded MB_YES. */
    mb_get_binary_int(mb_io_ptr->byteswapped, &header_buf[0], (void *)&record_size);
    mb_get_binary_int(mb_io_ptr->byteswapped, &header_buf[4], (void *)&id_word);

    checksum_present = (id_word & MBSYS_GSFNTV_CHECKSUM_FLAG_MASK) ? true : false;
    record_type = (int)(id_word & MBSYS_GSFNTV_DATATYPE_MASK);

    body_bytes_remaining = record_size;

    /* --- 2. skip the optional 4-byte checksum word --- */
    if (checksum_present) {
        read_len = (size_t)4;
        status = mb_fileio_get(verbose, mbio_ptr, checksum_buf, &read_len, error);
        if (status != MB_SUCCESS) {
            store->kind = MB_DATA_NONE;
            return status;
        }
        body_bytes_remaining -= 4;
        /* Checksum verification (modulo-32 sum) is optional for a read-only
         * module processing files already on disk; not implemented here. */
    }

    /* --- 3. read the remaining record body into a persistent, growable buffer --- */
    if ((unsigned int)mb_io_ptr->save6 < body_bytes_remaining) {
        status = mb_reallocd(verbose, __FILE__, __LINE__, (size_t)body_bytes_remaining,
                              (void **)&mb_io_ptr->saveptr1, error);
        if (status != MB_SUCCESS) {
            mb_io_ptr->save6 = 0;
            store->kind = MB_DATA_NONE;
            return status;
        }
        mb_io_ptr->save6 = (int)body_bytes_remaining;
    }

    body_buf = (char *)mb_io_ptr->saveptr1;

    read_len = (size_t)body_bytes_remaining;
    status = mb_fileio_get(verbose, mbio_ptr, body_buf, &read_len, error);
    if (status != MB_SUCCESS) {
        store->kind = MB_DATA_NONE;
        return status;
    }

    /* --- 4. dispatch on record type --- */
    switch (record_type) {
        case MBSYS_GSFNTV_REC_HEADER:
            status = mbr_gsfnativ_rd_header(verbose, body_buf, store, error);
            store->kind = MB_DATA_HEADER;
            break;

        case MBSYS_GSFNTV_REC_SWATHBATHYMETRYPING:
            status = mbr_gsfnativ_rd_ping(verbose, mbio_ptr, body_buf, (int)body_bytes_remaining, store, error);
            store->kind = MB_DATA_DATA;
            break;

        case MBSYS_GSFNTV_REC_ATTITUDE:
            status = mbr_gsfnativ_rd_attitude(verbose, mbio_ptr, body_buf, store, error);
            store->kind = MB_DATA_ATTITUDE;
            break;

        case MBSYS_GSFNTV_REC_COMMENT:
            /* not decoded in v1 scope, but tagged with the real comment kind */
            store->kind = MB_DATA_COMMENT;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SOUNDVELOCITYPROFILE:
            store->kind = MB_DATA_VELOCITY_PROFILE;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_PROCESSINGPARAMETERS:
            store->kind = MB_DATA_PROCESSING_PARAMETERS;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SENSORPARAMETERS:
            store->kind = MB_DATA_SENSOR_PARAMETERS;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_HISTORY:
            store->kind = MB_DATA_HISTORY;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_NAVIGATIONERROR:
        case MBSYS_GSFNTV_REC_HVNAVIGATIONERROR:
            store->kind = MB_DATA_NAVIGATION_ERROR;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SWATHBATHYSUMMARY:
            store->kind = MB_DATA_SUMMARY;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SINGLEBEAMSOUNDING:
            store->kind = MB_DATA_SINGLE_BEAM_PING;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        default:
            /* Unrecognized record type. The body has already been fully
             * consumed by step 3 above (byte offsets stay correct for the
             * next record); there is no generic "other" kind to fall back
             * to, so this is genuinely unintelligible. */
            store->kind = MB_DATA_NONE;
            status = MB_FAILURE;
            *error = MB_ERROR_UNINTELLIGIBLE;
            break;
    }

    return status;
}

/*--------------------------------------------------------------------*/
/* Header Record -- 12 bytes, plain text, no binary fields to swap. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_header(int verbose, char *buf, struct mbsys_gsfntv_struct *store, int *error) {
    memset(store->header.version, 0, MBSYS_GSFNTV_VERSION_SIZE);
    memcpy(store->header.version, buf, MBSYS_GSFNTV_VERSION_SIZE);

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Swath Bathymetry Ping Record. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_ping(int verbose, void *mbio_ptr, char *buf, int body_len,
                          struct mbsys_gsfntv_struct *store, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    struct mbsys_gsfntv_pingheader_struct *h = &(ping->header);
    bool swap = mb_io_ptr->byteswapped;
    int index = 0;
    int i;
    short s_tmp;

    /* --- ping header, exactly 56 bytes for GSF v03.01+ --- */
    mb_get_binary_int(swap, &buf[index], (void *)&h->time_sec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->time_nsec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->longitude_raw); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->latitude_raw); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&h->num_beams); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->center_beam); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->ping_flags = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->reserved = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->tide_corrector); index += 2;
    mb_get_binary_int(swap, &buf[index], (void *)&h->depth_corrector); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->heading_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->pitch_raw); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->roll_raw); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->heave); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->course_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->speed_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_int(swap, &buf[index], (void *)&h->height); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->separation); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->gps_tide_corrector); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&h->spare); index += 2;
    /* index now equals 56, verified by direct field-width summation */

    if (h->num_beams < 0 || h->num_beams > MBSYS_GSFNTV_MAX_BEAMS) {
        *error = MB_ERROR_UNINTELLIGIBLE;
        return MB_FAILURE;
    }

    /* --- decode the header's directly-usable scalar values --- */
    store->time_d = (double)h->time_sec + (double)h->time_nsec / 1.0e9;
    mb_get_date(verbose, store->time_d, store->time_i);

    ping->navlon = (double)h->longitude_raw / 10000000.0;
    ping->navlat = (double)h->latitude_raw / 10000000.0;
    ping->heading = (double)h->heading_raw / 100.0;
    ping->speed = (double)h->speed_raw / 100.0; /* knots */

    /* --- walk the remaining subrecords until the ping body is exhausted --- */
    while (index < body_len) {
        unsigned int subrecord_word;
        int subrecord_id, subrecord_size;

        if (index + 4 > body_len)
            break; /* malformed trailing bytes; stop rather than read past the buffer */

        mb_get_binary_int(swap, &buf[index], (void *)&subrecord_word); index += 4;

        /* TODO-VERIFY-VS-SPEC: this bit split (12-bit id, 20-bit size) is a
         * GSF-Spec question (Figure 4-6), not resolvable from MB-System
         * source; confirm against the actual GSF Spec PDF before trusting
         * this in production. */
        subrecord_id = (int)((subrecord_word >> 20) & 0x00000FFFu);
        subrecord_size = (int)(subrecord_word & 0x000FFFFFu);

        if (subrecord_size < 0 || index + subrecord_size > body_len)
            break; /* declared size runs past the record; stop defensively */

        switch (subrecord_id) {
            case MBSYS_GSFNTV_SUB_SCALEFACTORS:
                mbr_gsfnativ_rd_scalefactors(verbose, mbio_ptr, &buf[index], &ping->scalefactors, error);
                break;

            case MBSYS_GSFNTV_SUB_DEPTHARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_DEPTHARRAY, ping->depth, error);
                break;

            case MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY, ping->across_track, error);
                break;

            case MBSYS_GSFNTV_SUB_ALONGTRACKARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_ALONGTRACKARRAY, ping->along_track, error);
                break;

            case MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY, ping->amplitude, error);
                break;

            case MBSYS_GSFNTV_SUB_BEAMFLAGSARRAY:
                /* always 1 byte per beam, never scaled */
                for (i = 0; i < h->num_beams; i++)
                    ping->beam_flags[i] = (unsigned char)buf[index + i];
                break;

            default:
                /* any subrecord not in this module's v1 scope (travel time, beam
                 * angle, quality factor, receive heave, nominal depth, quality
                 * flags, signal-to-noise, beam-angle-forward, vertical/horizontal
                 * error, sensor-specific, imagery, etc.) -- already accounted for
                 * by advancing index below; simply not decoded */
                break;
        }

        index += subrecord_size;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Scale-factor subrecord. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_scalefactors(int verbose, void *mbio_ptr, char *buf,
                                  struct mbsys_gsfntv_scalefactors_struct *sf, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    int index = 0;
    int i;
    unsigned int id_and_flags;

    mb_get_binary_int(swap, &buf[index], (void *)&sf->num_factors); index += 4;

    if (sf->num_factors < 0)
        sf->num_factors = 0;
    if (sf->num_factors > MBSYS_GSFNTV_MAX_SCALEFACTORS)
        sf->num_factors = MBSYS_GSFNTV_MAX_SCALEFACTORS;

    for (i = 0; i < sf->num_factors; i++) {
        mb_get_binary_int(swap, &buf[index], (void *)&id_and_flags); index += 4;

        /* TODO-VERIFY-VS-SPEC: this split is a GSF-Spec question (Figure 4-7),
         * not resolvable from MB-System source; it is modeled on GSFlib's
         * documented "cflag" byte packing (field size high nibble,
         * compression flag low nibble) alongside the subrecord id. Confirm
         * against the actual GSF Spec PDF before trusting in production. */
        sf->factor[i].subrecord_id = (int)((id_and_flags >> 8) & 0x00FFFFFFu);
        sf->factor[i].compression_flag = (int)(id_and_flags & 0x0000000Fu);

        {
            int size_selector = (int)((id_and_flags >> 4) & 0x0000000Fu);
            if (size_selector == 1) sf->factor[i].field_size = 1;
            else if (size_selector == 3) sf->factor[i].field_size = 4;
            else sf->factor[i].field_size = 2; /* default / selector 2 */
        }

        mb_get_binary_float(swap, &buf[index], (void *)&sf->factor[i].multiplier); index += 4;
        mb_get_binary_float(swap, &buf[index], (void *)&sf->factor[i].offset); index += 4;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Generic scaled beam-array decoder. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_beamarray(int verbose, void *mbio_ptr, char *buf, int num_beams,
                               struct mbsys_gsfntv_scalefactors_struct *sf, int target_subrecord_id,
                               double *out_array, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    float multiplier = 1.0f, offset = 0.0f;
    int field_size = 2; /* default per-element width if no matching scale factor is found */
    int i, found = 0;
    int raw;
    short s_raw;

    for (i = 0; i < sf->num_factors; i++) {
        if (sf->factor[i].subrecord_id == target_subrecord_id) {
            multiplier = sf->factor[i].multiplier;
            offset = sf->factor[i].offset;
            field_size = sf->factor[i].field_size;
            found = 1;
            break;
        }
    }

    if (!found || multiplier == 0.0f)
        multiplier = (multiplier == 0.0f) ? 1.0f : multiplier; /* guard divide-by-zero */

    for (i = 0; i < num_beams; i++) {
        if (field_size == 4) {
            mb_get_binary_int(swap, &buf[4 * i], (void *)&raw);
        } else if (field_size == 1) {
            raw = (unsigned char)buf[i];
        } else {
            mb_get_binary_short(swap, &buf[2 * i], (void *)&s_raw);
            raw = s_raw;
        }

        out_array[i] = mbsys_gsfnative_decode_scaled(raw, multiplier, offset);
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Attitude Record. Decodes the record AND feeds every sample into
 * MBIO's generic asynchronous-attitude interpolation buffer via
 * mb_attint_add(), so that mb_attint_interp() can later be called (by
 * this module's own extract_nav() or by an application) to obtain
 * roll/pitch/heave interpolated onto any survey-record timestamp.
 * mb_attint_add()'s parameter order is CONFIRMED against mb_define.h:
 * int mb_attint_add(int verbose, void *mbio_ptr, double time_d,
 *                    double heave, double roll, double pitch, int *error); */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_attitude(int verbose, void *mbio_ptr, char *buf,
                              struct mbsys_gsfntv_struct *store, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    struct mbsys_gsfntv_attitude_struct *att = &(store->attitude);
    int index = 0;
    int i;
    short offset_raw; /* TODO-VERIFY-VS-SPEC: unit assumed milliseconds */
    short s_val;
    unsigned short us_val;
    int add_status, add_error;

    mb_get_binary_int(swap, &buf[index], (void *)&att->base_time_sec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&att->base_time_nsec); index += 4;
    att->base_time_d = (double)att->base_time_sec + (double)att->base_time_nsec / 1.0e9;

    mb_get_binary_short(swap, &buf[index], (void *)&s_val);
    att->num_measurements = (int)s_val;
    index += 2;

    if (att->num_measurements < 0)
        att->num_measurements = 0;
    if (att->num_measurements > MBSYS_GSFNTV_MAX_ATTITUDE)
        att->num_measurements = MBSYS_GSFNTV_MAX_ATTITUDE;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&offset_raw);
        att->sample_time_d[i] = att->base_time_d + (double)offset_raw / 1000.0; /* assumed ms */
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->pitch[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->roll[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->heave[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        us_val = (unsigned short)s_val;
        att->heading[i] = (double)us_val / 100.0;
    }
    index += 2 * att->num_measurements;

    store->time_d = att->base_time_d;
    mb_get_date(verbose, store->time_d, store->time_i);

    /* --- on-the-fly interpolation feed, confirmed parameter order --- */
    for (i = 0; i < att->num_measurements; i++) {
        add_error = MB_ERROR_NO_ERROR;
        add_status = mb_attint_add(verbose, mbio_ptr, att->sample_time_d[i],
                                    att->heave[i], att->roll[i], att->pitch[i], &add_error);
        if (add_status != MB_SUCCESS && verbose >= 1)
            fprintf(stderr, "mbr_gsfnativ_rd_attitude: mb_attint_add() failed, error %d\n", add_error);
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}
```
