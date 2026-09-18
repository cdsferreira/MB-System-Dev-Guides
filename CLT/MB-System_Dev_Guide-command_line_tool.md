# MB-System `mbiho` Developer's Guide (v4 — Beginner-Friendly Edition)

## A Note Before You Start

This guide assumes you can program (e.g. in Python) but have **little or no experience with C or C++**. Every time we use a C-specific concept — pointers, structs, header files, manual memory handling, compilation — we will stop and explain it in plain language, often with a Python comparison. If you already know C, skim the "C Concept" boxes and move on.

---

## Part 0: What is `mbiho`, and Why C?

`mbiho` is a small **command-line tool** that reads multibeam sonar ping data through MB-System's MBIO library and computes, for every valid depth sounding (called a "beam"), a rough estimate of vertical uncertainty (TVU) and horizontal uncertainty (THU), following the IHO S-44 standard formulas [web:1]. The output is a CSV file you can open in Excel, pandas, or any spreadsheet tool.

**Why is it written in C (well, C++ with a `.cc` extension)?** MB-System's core engine, MBIO, is a C library. Almost every existing MB-System tool (`mblist`, `mbinfo`, `mbclean`, etc.) is written to call directly into that library, so a new tool has to "speak" that same low-level language to reuse decades of file-format decoding logic. If you tried to write this in Python, you would either have to re-implement dozens of proprietary sonar file formats yourself, or wrap the C library with something like `ctypes`/`SWIG` — MB-System's own build system does not support that path, so C/C++ is the pragmatic choice here.

> **C Concept — Compiled vs. interpreted:** Python is interpreted: you run `python script.py` and it executes line by line, checking types as it goes. C is compiled: you first translate (`compile`) the whole `.cc` file into machine code (a binary), and only then run that binary. Mistakes like wrong types are usually caught at compile time, not at run time — which is why C compiler errors can look intimidating but actually save you from crashes later.

---

## Part 1: The Tool at a Glance

### 1.1 What it does

For every ping in an input multibeam file, `mbiho` visits every "beam" (one depth measurement, roughly analogous to one pixel in a depth image), checks if it's flagged as good or bad, and if good, computes:

- **TVU** (Total Vertical Uncertainty) using the S-44 formula \( TVU(d) = \sqrt{a^2 + (b \times d)^2} \) [web:1]
- **THU** (Total Horizontal Uncertainty), approximated as a fixed multiple of a horizontal-positioning sigma value

It writes one CSV row per beam: ping number, beam number, depth, beam angle, TVU, THU.

### 1.2 What it deliberately does *not* do

It does not build a full per-sensor error budget (heave sensor error, gyro error, latency, tide error, sensor offsets, etc.) the way commercial TPU engines do. It uses fixed coefficients per IHO survey order. This keeps the code short and the math easy to follow — a good first "real" MB-System tool to write.

### 1.3 Command-line only

`mbiho` has **no graphical interface**. You run it from a terminal, it reads command-line flags (like `-I` for input file), and it writes plain text to standard output (`stdout`) and error messages to standard error (`stderr`).

> **C Concept — stdout vs. stderr:** In Python, `print()` goes to stdout by default, and you'd use `print(..., file=sys.stderr)` for errors. In C, we use the function `printf()` for stdout and `fprintf(stderr, ...)` for stderr. Keeping data output (stdout) separate from diagnostic messages (stderr) means a user can redirect just the data into a file (`mbiho -I file.mb88 > output.csv`) without diagnostic clutter mixing in.

---

## Part 2: Core C Concepts You Need Before Reading the Code

If you're comfortable with Python but new to C, these five ideas will unblock almost everything in the source file.

### 2.1 Variables have a fixed type and fixed size

In Python, `x = 5` then `x = "hello"` is legal — the variable's type can change. In C, once you declare `int x;`, `x` can only ever hold an integer, and the compiler reserves a fixed chunk of memory (e.g. 4 bytes) for it. You must declare a variable's type before using it: `double depth;` reserves space for one floating-point number.

### 2.2 Pointers: variables that hold an address

This is the single biggest new idea. A pointer is a variable that stores *the memory address of another variable*, not a value directly.

```c
int x = 5;
int *p = &x;   // p now holds the ADDRESS of x
*p = 10;       // "dereference" p: go to that address and set the value to 10
               // now x is 10, even though we never wrote "x = 10"
```

- `&x` means "give me the address of x" (like asking "where does this variable live in memory?").
- `int *p` declares `p` as "a pointer to an int" — it stores an address, not a number.
- `*p` (dereferencing) means "go to the address stored in p, and read/write the value there."

Why does this matter for us? MB-System's functions need to give *you* several pieces of information at once (a status code, an array of depths, an error code...). C functions can only directly `return` one value, so instead they take pointers as arguments and **write their results through those pointers**. This is roughly like a Python function that mutates a mutable argument (e.g. appending to a list you passed in) instead of returning a new one.

```c
mb_read_init(verbose, file, format, pings, lonflip, bounds,
             btime_i, etime_i, speedmin, timegap,
             &mbio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, &error);
```

Every argument starting with `&` here is "please write your output into this variable for me."

### 2.3 Arrays and "double pointers" (`void **`)

A C array (e.g. `double depths[100]`) is just a contiguous block of 100 doubles in memory, and the array variable itself is really a pointer to the first element. When you see `void **mbio_ptr` in MB-System's function signatures, read it as "a pointer to a pointer" — it exists because `mb_read_init()` needs to *create* a new internal data structure and hand you back a pointer to it, so it needs the address of your pointer variable (`&mbio_ptr`) in order to set it.

### 2.4 Structs: like a lightweight Python class with only data

```c
struct mb_io_struct {
    int format;
    double depth_scale;
    ...
};
```

A `struct` bundles related variables together, similar to a Python `dataclass` with only fields, no methods. MBIO uses an internal struct (`mb_io_struct`) to hold everything about an open file (its format, current record, buffers, etc.), but as a tool author, you almost never touch that struct directly — you get an *opaque pointer* to it (`void *mbio_ptr`) and pass that pointer back into MBIO functions, which know how to interpret it. Think of it like a file handle in Python (`f = open(...)`) — you don't peek inside `f`'s internal state, you just pass `f` to `f.read()`.

### 2.5 Header files and why we `#include` them

A C **header file** (`.h`) contains declarations — "here is a function called `mb_read_init`, and here is exactly what arguments it expects" — without the actual implementation. This is like a Python function's signature/docstring without its body. The real implementation lives in a compiled library file (`.so`/`.a`) that gets linked in later. `#include <mb_status.h>` is roughly analogous to Python's `import mb_status`, except the compiler needs the header to check your function calls are correct *before* compiling, and the linker needs the actual library file to produce a runnable program.

> **Caveat carried over from earlier review:** the header `mb_io.h` you provided only declares the internal `mb_io_struct` and function-pointer *typedefs*; the actual `extern` prototypes for `mb_read_init()`, `mb_get_all()`, `mb_close()`, `mb_register_array()`, and `mb_error()` live in `mb_status.h`/`mb_process.h`, which we don't have. The signatures below are validated against `mb_io.h`'s internal typedefs (which mirror the public functions by convention), but you should confirm against your local `mb_status.h` before compiling.

### 2.6 Manual memory management (and why we mostly avoid it here)

Python's garbage collector frees memory for you automatically. In C, if you allocate memory manually (`malloc`), you must free it yourself (`free`), or you leak memory. Good news: `mbiho` mostly avoids this — MBIO's `mb_register_array()` handles allocation/growth of the beam-data arrays for us, and `mb_close()` frees them. We only use a few fixed-size local variables and arrays, so there's no manual `malloc`/`free` bookkeeping to worry about.

---

## Part 3: The Overall Program Flow

Before looking at code, here's the plan in pseudocode (this will look familiar if you've written a Python loop over records in a file):

```text
parse command-line arguments (input filename, format, survey order, etc.)
open the file with mb_read_init()
loop:
    read one ping with mb_get_all()
    if end of file: break
    for each beam in this ping:
        if beam is flagged bad: skip
        compute TVU using depth and order-specific a,b coefficients
        compute THU using a fixed sigma and multiplier
        print one CSV row
close the file with mb_close()
```

Every MB-System listing/analysis tool (`mblist`, `mbinfo`, ours) follows this exact skeleton — only the "do something per beam" step changes.

---

## Part 4: Command-Line Interface

### 4.1 Options

| Flag | Meaning | Example |
|---|---|---|
| `-I` | input file | `-I 0001_20260101_120000.mb88` |
| `-F` | MBIO format ID | `-F 88` |
| `-O` | IHO survey order (1=Special, 2=Order1a, 3=Order1b, 4=Order2) | `-O 1` |
| `-S` | fixed horizontal sigma in meters (used for THU) | `-S 1.5` |
| `-X` | output CSV file (optional; default is stdout) | `-X results.csv` |
| `-h` | print help and exit | |

> **C Concept — parsing command-line arguments:** In Python you might use `argparse`. C programs receive arguments as `int argc` (count) and `char *argv[]` (array of C-style strings) in `main()`. We use the standard C library function `getopt()` to loop through them, similar in spirit to `argparse` but far more manual — you write a `switch` statement yourself for each flag.

### 4.2 Example usage

```bash
mbiho -I survey_line_001.mb88 -F 88 -O 2 -S 1.2 -X uncertainty.csv
```

### 4.3 The output file — what exactly gets written, and how

This is the tool's actual deliverable, so it deserves a full explanation.

**What it is:** a plain-text CSV (comma-separated values) file, one row per *accepted* beam (bad/flagged beams are skipped entirely — they never appear as a row). It's meant to be opened directly in Excel, LibreOffice, or loaded with `pandas.read_csv()` for further plotting/QA, exactly like the output of `mblist`.

**Exact schema (columns, in order):**

| Column | Type | Description |
|---|---|---|
| `ping` | integer | Sequential ping counter within this run, starting at 0 |
| `beam` | integer | Beam index within the ping (0 = first beam in the array) |
| `depth_m` | float | Corrected depth for this beam, in meters, as extracted from the file |
| `beam_angle_deg` | float | Angle of this beam from vertical (nadir), in degrees, reconstructed from across-track distance and depth |
| `tvu_m` | float | Computed Total Vertical Uncertainty, in meters |
| `thu_m` | float | Computed Total Horizontal Uncertainty, in meters |

**Example rows:**

```text
ping,beam,depth_m,beam_angle_deg,tvu_m,thu_m
0,45,102.340,0.15,0.812,2.940
0,46,102.912,1.02,0.816,2.940
1,45,103.001,0.14,0.817,2.940
```

**How it's produced, mechanically:** the program builds each row as a formatted C string using `printf()`-family formatting (`%d` for integers, `%.3f` for floats with 3 decimal places), and either prints it directly to the terminal (default) or, if `-X FILE` is given, opens `FILE` with `fopen()` in write mode and writes each row with `fprintf()` instead. There is no buffering trick or binary format involved — it is literally one `fprintf` call per accepted beam, called once per loop iteration.

> **C Concept — `fopen`/`fprintf`/`fclose`:** This is C's equivalent of Python's `with open(file, "w") as f: f.write(...)`. C has no `with` block for automatic cleanup, so you must explicitly call `fclose(fp)` yourself at the end, or the file may not be fully written to disk. We do this once, right before calling `mb_close()`.

---

## Part 5: Full Source Code Walkthrough (`mbiho.cc`)

Below, each block is explained before you see the next one. The complete file is provided separately for download.

### 5.1 Includes and constants

```cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "mb_status.h"
#include "mb_format.h"
#include "mb_define.h"
```

`#include` pulls in declarations from both the **standard C library** (`cstdio` for `printf`/`fopen`, `cmath` for `sqrt`, etc. — note the `c` prefix is the C++ convention for C standard headers) and MB-System's own headers.

### 5.2 IHO S-44 coefficient table

```cpp
struct IHOCoefficients { double a, b; };
static const IHOCoefficients IHO_ORDERS[5] = {
    {0.0, 0.0},        // index 0 unused
    {0.25, 0.0075},    // 1 = Special Order
    {0.5,  0.013},     // 2 = Order 1a
    {0.5,  0.013},     // 3 = Order 1b
    {1.0,  0.023},      // 4 = Order 2
};
```

> **C Concept — arrays of structs:** This is like a Python `list` of `dataclass` instances, e.g. `[IHOCoefficients(a=0.25, b=0.0075), ...]`. We index into it with `IHO_ORDERS[order]` exactly like Python list indexing.

### 5.3 The TVU/THU formulas

```cpp
double compute_tvu(double depth, double a, double b) {
    return std::sqrt(a * a + (b * depth) * (b * depth));
}

double compute_thu(double sigma_h) {
    const double k = 2.45; // ~95% confidence radius factor
    return k * sigma_h;
}
```

These are ordinary functions, just like Python `def compute_tvu(depth, a, b): return math.sqrt(...)`. The only C-specific thing is the explicit `double` return-type declaration — C always requires you to state what type a function returns.

### 5.4 Opening the file: `mb_read_init()`

```cpp
int status = mb_read_init(verbose, input_file, format, pings_avg,
                           lonflip, bounds, btime_i, etime_i,
                           speedmin, timegap,
                           &mbio_ptr, &btime_d, &etime_d,
                           &beams_bath, &beams_amp, &pixels_ss, &error);
if (status != MB_SUCCESS) {
    fprintf(stderr, "mbiho: unable to open %s (error %d)\n", input_file, error);
    exit(EXIT_FAILURE);
}
```

Recall from Part 2.2: every argument prefixed with `&` is an "output" — `mb_read_init()` fills in `mbio_ptr` (our handle to the open file), the time bounds actually found in the file, and the number of beams/pixels this format provides. The function's actual return value (`status`) is just a success/failure code, checked with a normal C `if`.

### 5.5 The main read loop: `mb_get_all()`

```cpp
void *store_ptr = nullptr;
int ping_count = 0;

while (true) {
    int kind;
    status = mb_get_all(verbose, mbio_ptr, &store_ptr, &kind,
                         time_i, &time_d, &navlon, &navlat,
                         &speed, &heading, &distance, &altitude, &sonardepth,
                         &beams_bath, &beams_amp, &pixels_ss,
                         beamflag, bath, amp, bathacrosstrack, bathalongtrack,
                         ss, ssacrosstrack, ssalongtrack, comment, &error);

    if (status == MB_FAILURE && error == MB_ERROR_EOF) break;
    if (status != MB_SUCCESS) continue;   // skip bad/comment records
    if (kind != MB_DATA_DATA) continue;   // skip non-ping records (e.g. comments)

    process_ping(ping_count, beams_bath, beamflag, bath, bathacrosstrack,
                 a_coeff, b_coeff, sigma_h, out);
    ping_count++;
}
```

> **C Concept — `while (true)` and `break`:** identical to Python's `while True:` and `break`. We loop "forever" and explicitly break out when we detect end-of-file (`MB_ERROR_EOF`).

The bug we caught earlier is fixed here: `store_ptr` is declared as a real `void *` variable (initialized to `nullptr`, C++'s version of Python's `None`), and we pass its address `&store_ptr` so `mb_get_all()` can write MBIO's internal per-format storage pointer through it on every call — passing a bare `nullptr` directly (with no variable behind it) would have no address to write to, and would crash or silently corrupt memory.

### 5.6 Processing one ping: the per-beam loop

```cpp
void process_ping(int ping_num, int nbeams, char *beamflag, double *bath,
                   double *bathacrosstrack, double a, double b,
                   double sigma_h, FILE *out) {
    for (int i = 0; i < nbeams; i++) {
        if (!mb_beam_ok(beamflag[i])) continue;  // skip flagged/bad beams

        double depth = bath[i];
        double angle_deg = std::atan2(bathacrosstrack[i], depth) * 180.0 / M_PI;
        double tvu = compute_tvu(depth, a, b);
        double thu = compute_thu(sigma_h);

        fprintf(out, "%d,%d,%.3f,%.3f,%.3f,%.3f\n",
                ping_num, i, depth, angle_deg, tvu, thu);
    }
}
```

> **C Concept — `for (int i = 0; i < nbeams; i++)`:** the C equivalent of Python's `for i in range(nbeams):`. `bath[i]` indexes into the array exactly like Python's `bath[i]`, except C does **no bounds checking** — reading past the array's end doesn't raise an `IndexError`, it silently reads garbage memory. This is why `nbeams` (given to us by MBIO) must be trusted and never guessed.

`mb_beam_ok()` is a macro (a text-substitution shortcut, similar in *effect* to calling a tiny function, but resolved by the preprocessor before compilation) that checks a beam's flag byte and returns true/false for "is this beam usable."

### 5.7 Closing the file: `mb_close()`

```cpp
mb_close(verbose, &mbio_ptr, &error);
if (out != stdout) fclose(out);
```

Symmetric with opening: releases everything MBIO allocated internally for this file. We also close our own output file handle if we opened one with `-X`.

---

## Part 6: Building `mbiho` Into MB-System (CMake Integration)

### 6.1 File location

Place the source file at:

```
src/utilities/mbiho.cc
```

This is where MB-System keeps its command-line analysis utilities (`mblist.cc`, `mbinfo.cc`, `mbclean.cc`, etc. all live here) [web:2].

### 6.2 Why `.cc` and not `.c`

MB-System's utilities directory compiles everything with a C++ compiler, even code that is written in a mostly-C style, so that it can share C++-only helper libraries and maintain a single consistent toolchain across the project. Using `.cc` (rather than `.c`) tells CMake and your compiler to use C++ compilation rules for this file, matching every other file in `src/utilities/`.

> **C Concept — C vs C++ compilation:** C++ is a superset of C with extra features (classes, `nullptr`, function overloading, stricter type checking). Nearly all valid C code compiles fine as C++ with minor exceptions (e.g. C++ requires explicit casts C would allow implicitly). Since `mbiho.cc` avoids C++-only features anyway, it's "C code compiled by a C++ compiler" — you get C++'s stricter error-checking for free.

### 6.3 Editing `src/utilities/CMakeLists.txt`

Find the block that declares other single-file utility executables, and add an entry for `mbiho` following the same pattern:

```cmake
add_executable(mbiho mbiho.cc)
target_link_libraries(mbiho PRIVATE mbio ${MATH_LIB})
install(TARGETS mbiho DESTINATION ${CMAKE_INSTALL_BINDIR})
```

- `add_executable(mbiho mbiho.cc)` — tells CMake to compile `mbiho.cc` into an executable named `mbiho`. Think of this as CMake's equivalent of a Python `setup.py` entry-point declaration, except it also controls actual compilation, not just packaging.
- `target_link_libraries(...)` — links against the `mbio` library (so calls like `mb_read_init()` resolve to real code) and the math library (needed for `sqrt`, `atan2`).
- `install(TARGETS mbiho DESTINATION ...)` — tells `cmake --install` where to copy the final binary (typically alongside `mblist`, `mbinfo`, etc. in your install prefix's `bin/`).

### 6.4 Rebuilding

```bash
cd build
cmake --build . --target mbiho
cmake --install . --component runtime
```

If `cmake --build .` (no `--target`) is used instead, `mbiho` will be built as part of the full MB-System build since it's now listed in `CMakeLists.txt`.

---

## Part 7: Writing and Installing the Man Page

### 7.1 The man page source (`mbiho.1`)

Place this file at `src/utilities/mbiho.1` (or your project's conventional man-page directory), following the same `groff`/`man` macro format as other MB-System man pages:

```groff
.TH MBIHO 1 "September 2026" "MB-System 5.8" "MB-System User's Manual"
.SH NAME
mbiho \- compute rough IHO S-44 TVU/THU uncertainty estimates from multibeam ping data
.SH SYNOPSIS
.B mbiho
.B \-I
.I file
[
.B \-F
.I format
] [
.B \-O
.I order
] [
.B \-S
.I sigma
] [
.B \-X
.I outputfile
] [
.B \-h
]
.SH DESCRIPTION
.B mbiho
reads multibeam bathymetry data through the MBIO library and computes,
for each valid (unflagged) beam, a Total Vertical Uncertainty (TVU) and
Total Horizontal Uncertainty (THU) estimate following the IHO S-44
standard depth-dependent error model. Results are written as CSV,
one row per beam, to standard output or to a file specified with
.BR \-X .
.SH OPTIONS
.TP
.BI \-I " file"
Input multibeam data file.
.TP
.BI \-F " format"
MBIO format identifier for the input file.
.TP
.BI \-O " order"
IHO S-44 survey order: 1=Special, 2=Order 1a, 3=Order 1b, 4=Order 2.
.TP
.BI \-S " sigma"
Fixed horizontal positioning sigma, in meters, used to derive THU.
.TP
.BI \-X " outputfile"
Write CSV output to
.I outputfile
instead of standard output.
.TP
.B \-h
Print a usage summary and exit.
.SH EXAMPLES
.nf
mbiho \-I survey_line_001.mb88 \-F 88 \-O 2 \-S 1.2 \-X uncertainty.csv
.fi
.SH SEE ALSO
.BR mblist (1),
.BR mbinfo (1),
.BR mbio (3)
.SH AUTHOR
Generated as part of the MB-System developer's guide series.
```

> **C Concept aside:** man pages aren't C code, but MB-System treats them as source files that ship with the build, which is why they get their own `install()` rule below, just like the compiled binary.

### 7.2 What the man page looks like at the terminal

Once installed, running `man mbiho` renders roughly as follows:

```text
MBIHO(1)                MB-System User's Manual                MBIHO(1)

NAME
       mbiho - compute rough IHO S-44 TVU/THU uncertainty estimates
       from multibeam ping data

SYNOPSIS
       mbiho -I file [-F format] [-O order] [-S sigma] [-X outputfile] [-h]

DESCRIPTION
       mbiho reads multibeam bathymetry data through the MBIO library
       and computes, for each valid (unflagged) beam, a Total Vertical
       Uncertainty (TVU) and Total Horizontal Uncertainty (THU)
       estimate following the IHO S-44 standard depth-dependent error
       model. Results are written as CSV, one row per beam, to
       standard output or to a file specified with -X.

OPTIONS
       -I file       Input multibeam data file.
       -F format     MBIO format identifier for the input file.
       -O order      IHO S-44 survey order: 1=Special, 2=Order 1a,
                      3=Order 1b, 4=Order 2.
       -S sigma       Fixed horizontal positioning sigma, in meters,
                      used to derive THU.
       -X outputfile  Write CSV output to outputfile instead of
                      standard output.
       -h             Print a usage summary and exit.

EXAMPLES
       mbiho -I survey_line_001.mb88 -F 88 -O 2 -S 1.2 -X uncertainty.csv

SEE ALSO
       mblist(1), mbinfo(1), mbio(3)

AUTHOR
       Generated as part of the MB-System developer's guide series.

MB-System 5.8                September 2026                          1
```

### 7.3 Installing the man page via CMake

In `src/utilities/CMakeLists.txt`, alongside the `mbiho` target:

```cmake
install(FILES mbiho.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
```

`CMAKE_INSTALL_MANDIR` is a standard CMake variable pointing at the install prefix's `share/man` directory; appending `man1` places it where `man mbiho` will find it (section 1 = user commands). Rebuild and reinstall with the same `cmake --build`/`cmake --install` commands from Part 6.4, then confirm with `man mbiho`.

---

## Part 8: Testing Checklist

- Compile cleanly with no warnings: `cmake --build . --target mbiho`
- Run against a known small test file and manually verify a few TVU/THU values by hand-computing the S-44 formula for one beam's depth
- Confirm flagged/bad beams never appear in the output CSV
- Confirm `-X` writes to the given file and stdout is empty when `-X` is used
- Confirm `-h` prints usage and exits without requiring `-I`
- Run `man mbiho` after install and check formatting renders correctly
- **Known caveat:** if compilation fails on `mb_read_init`/`mb_get_all`/`mb_close` signatures, check `mb_status.h` in your local MB-System source tree — `mb_io.h` alone does not contain their `extern` prototypes, only internal struct/typedef definitions

---

## Part 9: References

1. International Hydrographic Organization, "IHO Standards for Hydrographic Surveys, S-44," 6th Edition — https://iho.int/uploads/user/pubs/standards/s-44/S-44_Edition_6.1.0.pdf
2. MB-System source repository, `src/utilities` directory — https://github.com/dwcaress/MB-System/tree/master/src/utilities
3. MB-System documentation and man pages — https://www.mbari.org/technology/mb-system/
4. CMake documentation, `add_executable`, `target_link_libraries`, `install` — https://cmake.org/cmake/help/latest/
5. GNU `man-pages` project, man page format conventions — https://man7.org/linux/man-pages/man7/man-pages.7.html
