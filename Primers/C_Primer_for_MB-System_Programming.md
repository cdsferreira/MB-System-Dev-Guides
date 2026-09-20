# C Primer for MB-System Programming

*A beginner-to-intermediate introduction to the C programming language, taught entirely through the source code of MB-System.*

---

## How to Use This Primer

This document assumes **no prior programming experience**. Every concept — even ones that seem obvious to an experienced programmer — is explained from first principles: what a variable actually is in memory, what a compiler does, what a pointer really points *at*. At the same time, every example, once we move past the very first few pages, is drawn from or modeled directly on the real source code of **MB-System**, so that the C you learn here is immediately useful for reading, understanding, and eventually modifying MB-System itself.

MB-System is an open-source software package for processing bathymetry and backscatter imagery from multibeam, interferometric, and sidescan sonars. It was originally developed at the Lamont-Doherty Earth Observatory of Columbia University, and **is now a collaborative effort between the Monterey Bay Aquarium Research Institute (MBARI), the Center for Marine Environmental Sciences (MARUM) at Universität Bremen, and the Center for Coastal and Ocean Mapping (CCOM) at the University of New Hampshire** [web:16]. Its core I/O library, MBIO, is written entirely in C, which makes it an ideal teaching corpus: large and realistic enough to show genuine engineering practice, but not obscured by C++ abstractions.

This primer does not divide itself into "Chapter 1, Chapter 2" the way a conventional textbook would. Instead it is organized into numbered **Parts**, each covering a coherent slice of the C language or the MB-System codebase, building continuously from absolute basics to intermediate, real-world competence. Read it start to finish, in order — later parts depend on earlier ones.

---

## Table of Contents

- Part 1 — What Programming Actually Is
- Part 2 — Setting Up Your MB-System Development Environment
- Part 3 — Anatomy of a C Program
- Part 4 — Variables, Types, and How Data Lives in Memory
- Part 5 — Operators and Expressions
- Part 6 — Input and Output
- Part 7 — Conditionals: Making Decisions
- Part 8 — Loops: Repeating Work
- Part 9 — Functions
- Part 10 — Arrays
- Part 11 — Pointers
- Part 12 — Strings
- Part 13 — Structures
- Part 14 — Dynamic Memory Allocation
- Part 15 — File Input and Output
- Part 16 — The Preprocessor
- Part 17 — Multi-File Programs and the CMake Build System
- Part 18 — MB-System's Core Conventions in Depth
- Part 19 — Putting It Together: Reading a Real Multibeam File
- Part 20 — Intermediate Topics
- Part 21 — Debugging and Tools
- Part 22 — Where to Go Next

---

## Part 1 — What Programming Actually Is

### 1.1 What a Computer Actually Does

At the lowest level, a computer is a machine that repeatedly does one thing: it reads a small numeric instruction from memory, performs a tiny operation (add two numbers, move a value, compare two values, jump to a different instruction), and moves on to the next instruction. These numeric instructions are called **machine code**. A processor has no concept of "variables," "loops," or "functions" — those are ideas that exist purely to help *humans* organize instructions; underneath, everything is numbers being moved around and compared.

Writing machine code by hand is possible but extremely tedious and error-prone, so programmers write in **higher-level languages** instead — languages with words, structure, and abstractions that map, more or less directly, onto sequences of machine instructions. C is one such language: often described as a "low-level high-level language" because it lets you work close to the machine (manipulating raw memory addresses, individual bytes, exact numeric types) while still using readable syntax like `if`, `for`, and named variables.

### 1.2 What a Compiler Does

A **compiler** is a program that translates your human-readable C source code (plain text files ending in `.c` and `.h`) into machine code that the processor can actually execute. This translation happens in several stages:

1. **Preprocessing** — textual substitutions (covered fully in Part 16): expanding `#include` files, replacing `#define` macros.
2. **Compilation** — translating the preprocessed C code into low-level assembly/object code, checking along the way that your code obeys C's grammar and type rules.
3. **Assembly** — turning that intermediate code into actual machine instructions, producing an **object file** (`.o`).
4. **Linking** — combining one or more object files, plus any libraries they depend on, into a single runnable **executable**.

You will use a compiler called `gcc` (GNU Compiler Collection) or `clang` (LLVM's compiler, which macOS ships as the `gcc`/`cc` command by default) to do all four stages, usually with a single command, exactly as we will practice in Part 2.

### 1.3 Why C, Specifically, for MB-System

C was designed in the early 1970s at Bell Labs, originally to write the Unix operating system itself. Its defining traits — direct memory access, minimal runtime overhead, predictable performance, and portability across very different hardware — are exactly the traits that matter for scientific software that has to process large binary sonar files efficiently, run identically on Linux workstations, macOS laptops, and shipboard acquisition computers, and interoperate with decades of accumulated format-specific code. MB-System's core I/O library, MBIO, is written entirely in C for precisely these reasons; understanding C is therefore not optional if you want to read, debug, or extend that library — there is no shortcut through a "simpler" language, because the actual code you need to work with is not written in one.

### 1.4 A Note on What You Already Know

Given your background in Python and in patching C/C++ codebases, some of what follows (variables, loops, functions as concepts) will feel familiar immediately. What is different — and where this primer spends real, careful time — is everything related to **memory**: how C makes you manage it explicitly, how pointers work, how arrays and pointers relate, and how structures are laid out byte-by-byte in memory. Python hides all of this from you deliberately. C does not hide any of it. That is both C's biggest source of power and its biggest source of bugs, and understanding it thoroughly is the actual point of this primer.

---

## Part 2 — Setting Up Your MB-System Development Environment

### 2.1 Installing a C Compiler

On macOS, install Apple's Command Line Tools, which provide Clang (invoked as `cc`, `gcc`, or `clang`):

```sh
xcode-select --install
```

Verify:

```sh
cc --version
```

On Linux, install GCC through your distribution's package manager, e.g. on Debian/Ubuntu: `sudo apt install build-essential`.

### 2.2 Installing CMake and Dependencies

MB-System's build system is **CMake**. Current MB-System releases have consolidated on CMake as the supported way to configure and build the project, with the legacy Autotools (`configure`/`Makefile.am`) path considered obsolete and no longer the recommended entry point for new development [web:22][web:25]. This primer therefore uses CMake exclusively, and every build instruction below assumes it.

Install CMake and MB-System's external library dependencies (NetCDF, GMT, Proj, FFTW; Qt or FLTK if you plan to build GUI tools later) via Homebrew on macOS:

```sh
brew install cmake netcdf gmt proj fftw qt
```

On Debian/Ubuntu:

```sh
sudo apt install cmake libnetcdf-dev gmt libproj-dev libfftw3-dev qtbase5-dev
```

### 2.3 Cloning the Source Repository

```sh
git clone https://github.com/dwcaress/MB-System.git
cd MB-System
```

At the top level you will find, among other things:

```
MB-System/
├── src/
├── man/
├── share/
├── ChangeLog.md
├── README.md
└── CMakeLists.txt
```

`CMakeLists.txt` at the root, and inside every source subdirectory, is the single, authoritative description of how the project builds. There is no `configure.ac` step to run, no `automake`/`autoreconf` bootstrap — CMake reads these `CMakeLists.txt` files directly.

### 2.4 Building MB-System With CMake

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/mbsystem ..
cmake --build . -j4
cmake --install .
```

Walking through each command:

- `mkdir build && cd build` — CMake strongly encourages **out-of-source builds**: all generated files (object files, Makefiles or Ninja files, CMake's own cache) live in a separate `build/` directory, keeping the source tree itself completely clean. If anything ever goes wrong, you can delete the entire `build/` directory and start over with zero risk to your source checkout.
- `cmake -DCMAKE_INSTALL_PREFIX=/usr/local/mbsystem ..` — runs CMake's *configuration* step. It reads `CMakeLists.txt` (found via `..`, the parent directory), detects your compiler and installed dependencies, and generates the actual build files (by default, Makefiles on macOS/Linux). `-D` sets a CMake variable; here we set the install location.
- `cmake --build . -j4` — runs the actual compilation, using 4 parallel jobs. This is a portable wrapper around whatever underlying build tool CMake generated for (Make, Ninja, Xcode, …), so this exact command works regardless of platform.
- `cmake --install .` — copies the compiled libraries, headers, and executables into the prefix you chose.

A full build compiles the entire MBIO library plus dozens of command-line utilities and can take a while; you only need to do it once, then again whenever you pull significant upstream changes.

### 2.5 A Working Directory for This Primer's Examples

Create a separate directory, outside the MB-System checkout, for the small programs you will write while working through this primer:

```sh
mkdir -p ~/mbsystem-c-primer
cd ~/mbsystem-c-primer
```

Every code listing from here on assumes you are compiling from a directory like this, pointing your compiler at the headers and libraries installed in Section 2.4.

---

## Part 3 — Anatomy of a C Program

### 3.1 The Smallest Possible C Program

```c
int main(void) {
    return 0;
}
```

This program does nothing visible, but every part of it matters. `int` is the **return type** of the function `main`: the value this function hands back to the operating system when it finishes. By convention, `0` means "success," and any nonzero value signals some kind of error — a convention used throughout MB-System's own command-line tools. `main` is a special function name: it is where every C program's execution begins, no matter how many other functions the program contains. `(void)` states that this function accepts no arguments. The curly braces `{ }` mark the **body** of the function — the block of statements that runs when the function executes. `return 0;` ends the function and supplies the return value; every C statement ends with a semicolon `;`, which tells the compiler "this statement is complete."

### 3.2 Comments

Text meant for human readers, ignored entirely by the compiler:

```c
/* This is a multi-line comment.
   It can span as many lines as you like. */

// This is a single-line comment (C99 and later, universally supported today)
```

MB-System's source, having originated in the early 1990s, uses `/* ... */` style comments extensively and consistently, including elaborate header-block comments at the top of every file describing its author, purpose, and revision history. You will see this convention constantly once you start reading real source.

### 3.3 Whitespace and Formatting

C almost entirely ignores whitespace (spaces, tabs, newlines) between tokens — the language would accept the entire smallest-program example above written on a single line. Formatting, indentation, and line breaks exist purely for human readability, and different projects adopt different conventions. MB-System's code style favors four-space indentation and places opening braces at the end of the same line as the statement that introduces a block (`if (x) {`), a style you should match if you ever contribute a patch upstream.

### 3.4 Statements and Blocks

A **statement** is a single complete instruction, ending in `;`. A **block** is a sequence of statements enclosed in `{ }`, treated as a single unit — used as the body of functions, `if`s, and loops:

```c
int main(void) {
    int x;       /* statement 1 */
    x = 5;       /* statement 2 */
    return 0;    /* statement 3 */
}
```

---

## Part 4 — Variables, Types, and How Data Lives in Memory

### 4.1 What a Variable Actually Is

A variable is a named location in the computer's memory (RAM) that holds a value. When you write:

```c
int beam_count = 24;
```

you are asking the compiler to reserve a small block of memory (on almost all modern systems, 4 bytes for an `int`), label that block with the name `beam_count` for the rest of your program's readability, and store the number `24` into it, encoded in binary. Memory itself is just an enormous sequence of numbered slots, each holding one **byte** (8 bits). Every variable you declare occupies some specific range of byte-numbered addresses; a variable's **type** tells the compiler (and, indirectly, you) exactly how many bytes it occupies and how to interpret the bits stored there — as a whole number, a fraction, a character, and so on.

### 4.2 Bits, Bytes, and Number Systems

A **bit** is a single binary digit: 0 or 1. A **byte** is 8 bits grouped together, giving 2⁸ = 256 possible values (0 through 255 if interpreted as an unsigned number). Computer memory, and the binary sonar file formats MB-System reads, are fundamentally sequences of bytes. Programmers frequently express byte values in **hexadecimal** (base 16, digits 0–9 then A–F) because two hex digits map exactly onto one byte (`0xFF` = 255 = all 8 bits set), which is far more compact and pattern-friendly than binary or decimal when inspecting raw file contents — exactly what you do constantly when working with KMALL or GSF records at the byte level.

```c
int decimal_value = 255;
int hex_value      = 0xFF;   /* the 0x prefix marks a hexadecimal literal */
int binary_value   = 0b11111111; /* binary literal, GCC/Clang extension */

printf("%d %d %d\n", decimal_value, hex_value, binary_value);  /* prints: 255 255 255 */
```

All three lines store the identical value; only the *notation* used to write the literal in source code differs. Understanding this equivalence matters directly when you read binary-format parsing code that manipulates raw bytes using hex constants and bitwise operators (covered in Part 20).

### 4.3 The Basic Built-In Types

| Type | Typical size | Range (typical) | Use |
|---|---|---|---|
| `char` | 1 byte | -128 to 127 (signed) | a single character, or a tiny integer |
| `short` | 2 bytes | -32,768 to 32,767 | small integers |
| `int` | 4 bytes | about ±2.1 billion | general-purpose whole numbers |
| `long` | 8 bytes (64-bit Unix) | very large whole numbers | large counts, file offsets |
| `unsigned int` | 4 bytes | 0 to about 4.3 billion | counts that are never negative |
| `float` | 4 bytes | ~7 significant decimal digits | single-precision decimals |
| `double` | 8 bytes | ~15–16 significant decimal digits | double-precision decimals |

Every type's exact size can technically vary by platform (C guarantees only minimums, not exact sizes), which is why MB-System, like most serious C projects, also uses explicitly-sized types from `<stdint.h>` — `int32_t`, `uint16_t`, `int64_t`, and so on — whenever a binary file format specifies an exact byte width for a field. When you are writing a format reader that must interpret a manufacturer's binary record layout byte-for-byte, "an `int` is usually 4 bytes on my machine" is not good enough; you need a type that is *guaranteed* to be exactly 4 bytes everywhere, and `int32_t` provides that guarantee.

```c
#include <stdint.h>

int32_t  ping_number;     /* guaranteed exactly 4 bytes, signed  */
uint16_t beam_count;      /* guaranteed exactly 2 bytes, unsigned */
uint8_t  quality_flag;    /* guaranteed exactly 1 byte, unsigned  */
```

### 4.4 Declaring and Initializing Variables

**Declaration** reserves memory and gives it a name and type. **Initialization** gives it a starting value at the moment of declaration. You can do both together or separately:

```c
int beam_count;          /* declared, but value is currently indeterminate/garbage */
beam_count = 256;        /* now initialized, via a separate assignment */

double latitude = 54.0797;   /* declared and initialized in one statement */
```

An uninitialized local variable's value is **whatever bits happened to already be in that memory location** — leftover from whatever used that memory before. Reading an uninitialized variable before assigning it a real value is one of the most common and dangerous C bugs, because the program may appear to work (if the leftover bits happen to look reasonable) and then fail unpredictably later on a different run, a different machine, or a different compiler optimization level. **Always initialize variables at declaration whenever practical.**

### 4.5 Constants

A value that must never change during execution can be declared `const`:

```c
const double SOUND_SPEED_DEFAULT_MPS = 1500.0;
```

The compiler will now reject any later attempt to assign a new value to `SOUND_SPEED_DEFAULT_MPS`, catching a class of bugs where a "constant" is accidentally overwritten somewhere deep in a large program. MB-System headers define many `const`-style and `#define`-style constants (covered in Part 16) for exactly this reason — default sound speed, maximum beam counts, buffer sizes — values that should be named, documented once, and never silently mutated.

### 4.6 Why MB-System Chooses `double` for Navigation and `float` for Bathymetry Arrays

This is a real engineering decision embedded throughout the codebase, not an arbitrary stylistic quirk. Navigation quantities — latitude, longitude, heading, and especially time — require `double` precision because `float`'s roughly 7 significant decimal digits are not enough to hold a longitude value's integer part *and* sub-meter fractional precision simultaneously; you would silently lose precision exactly where it matters most. `double`'s ~15–16 significant digits comfortably cover full-precision coordinates and time-since-epoch values.

Depth and beam arrays, in contrast, are frequently stored as `float`, mainly for memory and I/O efficiency. A single modern multibeam ping can carry hundreds of beams, and a full survey file can hold hundreds of thousands of pings; storing every value as `double` would roughly double memory footprint and file size for a numerical-precision gain far below the sonar's own real-world measurement uncertainty. The general lesson, which recurs throughout this primer: **choose the smallest type that does not discard information your data actually contains.**

### 4.7 A Worked Example

```c
#include <stdio.h>

int main(void) {
    int    beam_number     = 127;
    double latitude_deg    = 53.0793;    /* Bremen-area latitude */
    double longitude_deg   = 8.8017;
    float  depth_m         = 23.457f;    /* the f suffix marks a float literal, not double */
    char   sensor_name[32] = "EM710";    /* a 32-byte buffer holding a short string */

    printf("Beam %d on sensor %s at (%.6f, %.6f): depth %.3f m\n",
           beam_number, sensor_name, latitude_deg, longitude_deg, depth_m);

    return 0;
}
```

`char sensor_name[32]` is an **array** of 32 `char` values — the standard C idiom for a fixed-length string buffer, used constantly throughout MB-System for filenames, format names, and comment fields (arrays are covered fully in Part 10). The `f` suffix on `23.457f` tells the compiler this literal is already single-precision; without it, `23.457` defaults to `double`, and assigning it to a `float` variable involves an implicit, silently narrowing conversion — legal, but worth being deliberate about.

### 4.8 `printf` Format Specifiers

| Specifier | Type | Typical use |
|---|---|---|
| `%d` | `int` | beam counts, format IDs |
| `%ld` | `long` | file offsets, record counts |
| `%u` | `unsigned int` | counts guaranteed non-negative |
| `%f` | `float`/`double` | depths, angles |
| `%.3f` | float/double, 3 decimals | depth to millimeter precision |
| `%s` | `char *` (string) | sensor/format names |
| `%c` | `char` | a single character |
| `%x` | `int`, hexadecimal | raw byte/record-ID inspection |

Mismatching a format specifier and the actual type of the argument compiles (often with only a warning) but produces garbage output or, in worse cases, a crash — `printf` trusts you completely to describe the argument types correctly.

---

## Part 5 — Operators and Expressions

### 5.1 Arithmetic Operators

`+  -  *  /  %` — the last, modulo, gives the integer remainder of division and is only defined for integer operands.

### 5.2 The Integer Division Trap

```c
int total_beams = 7;
int good_beams  = 4;

double fraction_wrong = good_beams / total_beams;        /* WRONG: evaluates to 0.0 */
double fraction_right = (double)good_beams / total_beams; /* correct: 0.5714... */
```

Because both `good_beams` and `total_beams` are `int`, the division `good_beams / total_beams` happens entirely in **integer arithmetic first**, truncating any fractional part, producing `0` — and only afterward is that already-wrong `0` converted to `double` for storage. The fix is an explicit **cast**, `(double)good_beams`, which forces the division itself to be carried out in floating point. This is one of the single most common real-world C bugs, and it matters directly for statistics MB-System tools compute, such as the percentage of good versus flagged beams in a swath file.

### 5.3 Comparison and Logical Operators

`==  !=  <  >  <=  >=` for comparison; `&&  ||  !` for logical AND, OR, NOT.

A classic beginner trap: `=` is assignment, `==` is comparison.

```c
if (depth = 0) { ... }   /* almost certainly a bug: assigns 0 to depth, tests the result */
if (depth == 0) { ... }  /* what was actually meant: compares depth to 0 */
```

The first line *compiles* — it assigns `0` to `depth`, and the value of an assignment expression is the value assigned, so the `if` then tests `0`, which C treats as "false," so the branch never runs. Always double-check whenever you see a single `=` inside a condition.

### 5.4 Assignment Operators and Increment/Decrement

C provides compact combined assignment operators:

```c
int good_count = 0;
good_count += 1;    /* same as: good_count = good_count + 1; */
good_count++;       /* same as: good_count += 1; */
good_count--;       /* same as: good_count -= 1; */
```

`++` and `--` come in two forms with different behavior when used inside a larger expression: `x++` (post-increment) evaluates to `x`'s *original* value, then increments; `++x` (pre-increment) increments first, then evaluates to the *new* value. Inside a plain standalone statement like `good_count++;` the difference is invisible; it becomes important once increment expressions appear inside more complex expressions (a subtlety we flag but deliberately avoid relying on anywhere in this primer's own examples, for clarity).

### 5.5 Operator Precedence

Just like ordinary arithmetic, C operators have a defined precedence (`*` and `/` bind tighter than `+` and `-`, for example), and parentheses always override it. When in doubt, add parentheses — it costs nothing and removes ambiguity for future readers, including your future self:

```c
double angle_rad = beam_angle_deg * 3.14159265358979 / 180.0;   /* works, but... */
double angle_rad2 = (beam_angle_deg * 3.14159265358979) / 180.0; /* identical, clearer */
```

### 5.6 A Worked Geometry Example

A core operation in multibeam processing converts a beam's angle and slant range into vertical depth and horizontal across-track distance, under a simplifying flat-seafloor, constant-sound-speed assumption (the real MB-System code in `src/mbaux/` ray-traces through a full sound-velocity profile instead, which we approach in Part 20):

```c
#include <stdio.h>
#include <math.h>

int main(void) {
    double beam_angle_deg = 32.5;   /* angle from vertical, degrees */
    double slant_range_m  = 45.2;   /* one-way slant range, meters */

    double angle_rad = beam_angle_deg * M_PI / 180.0;

    double depth_m        = slant_range_m * cos(angle_rad);
    double across_track_m = slant_range_m * sin(angle_rad);

    printf("Beam angle %.2f deg, range %.2f m -> depth %.3f m, across-track %.3f m\n",
           beam_angle_deg, slant_range_m, depth_m, across_track_m);

    return 0;
}
```

`M_PI`, `cos()`, and `sin()` all come from `<math.h>`, and require linking with `-lm` (Part 17 covers compilation flags in full). This is original teaching code exercising the operators just introduced, functionally similar in spirit to the geometry conversions real swath-processing code performs, without reproducing any actual MB-System source text.

---

## Part 6 — Input and Output

### 6.1 Output With `printf`

Already used above; `printf` writes formatted text to the terminal (**standard output**). It takes a format string containing `%`-specifiers, followed by one argument per specifier, in order.

### 6.2 Input With `scanf`

`scanf` reads formatted input from the terminal (**standard input**):

```c
#include <stdio.h>

int main(void) {
    double depth_m;

    printf("Enter a depth in meters: ");
    scanf("%lf", &depth_m);   /* %lf for double; note the & (address-of) */

    printf("You entered: %.2f m\n", depth_m);
    return 0;
}
```

Note the `&` before `depth_m` — `scanf` needs to know *where* in memory to write the value it reads, not the current (irrelevant, possibly garbage) value already stored there. This is your first hands-on use of the address-of operator, which Part 11 explains fully. `scanf` is convenient for small teaching examples but is rarely used in real MB-System tools, which almost universally take their input via **command-line arguments** and **files** rather than interactive prompts — appropriate for programs meant to run inside scripted, automated survey-processing pipelines rather than be operated interactively.

### 6.3 Command-Line Arguments

Every C program's `main` function can optionally accept two parameters that give it access to whatever was typed after the program's name on the command line:

```c
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Program name: %s\n", argv[0]);
    printf("Number of arguments (including program name): %d\n", argc);

    for (int i = 1; i < argc; i++) {
        printf("  argument %d: %s\n", i, argv[i]);
    }

    return 0;
}
```

`argc` ("argument count") tells you how many strings were passed, *including* the program's own name as `argv[0]`. `argv` ("argument vector") is an array of those strings (`char *` values — pointers to characters, covered fully in Parts 11–12). Running `./myprogram file1.all -V` gives `argc == 3`, with `argv[0] == "./myprogram"`, `argv[1] == "file1.all"`, `argv[2] == "-V"`. This exact mechanism is how every MB-System command-line tool — `mbinfo`, `mbgrid`, `mbclean` — receives its filename and option flags, typically parsed with the standard `getopt` function rather than a hand-rolled loop, once option sets grow past a handful of flags.

---

## Part 7 — Conditionals: Making Decisions

### 7.1 `if`, `else if`, `else`

```c
double depth_m = 1523.7;

if (depth_m < 0.0) {
    printf("Invalid: negative depth\n");
} else if (depth_m < 20.0) {
    printf("Shallow water regime\n");
} else if (depth_m < 200.0) {
    printf("Continental shelf regime\n");
} else {
    printf("Deep water regime\n");
}
```

C evaluates each condition in order, top to bottom, and runs the block belonging to the *first* one that is true, skipping all the rest — `else if`/`else` are not independently re-checked once an earlier branch has matched.

### 7.2 What Counts as "True" in C

C has no dedicated built-in boolean type in its classic form (a true `bool` type was only added, via `<stdbool.h>`, in the C99 standard, and much of MB-System's older code predates relying on it, preferring self-defined `MB_YES`/`MB_NO` constants instead). Any numeric value of `0` is treated as **false**; *any* nonzero value — `1`, `-1`, `42`, all count — is treated as **true**. This is why `if (depth_m)` alone (without an explicit comparison) is legal C, testing only whether `depth_m` is nonzero — a compact idiom you will see, and should use sparingly and only when it is genuinely clearer than an explicit comparison.

### 7.3 `switch`

`switch` dispatches on a small integer (or `char`) value, and is well suited to exactly the kind of "given a numeric code, decide what to do" logic that MBIO faces when choosing, from a numeric format identifier, which reader/writer to call:

```c
#include <stdio.h>

const char *format_name(int format_id) {
    switch (format_id) {
        case 88:
            return "EM710/EM302/EM122 family (Kongsberg .all)";
        case 261:
            return "Kongsberg KMALL";
        case 121:
            return "Reson 7k";
        default:
            return "Unknown/unsupported format";
    }
}

int main(void) {
    int test_ids[] = {88, 261, 121, 999};

    for (int i = 0; i < 4; i++) {
        printf("Format %d -> %s\n", test_ids[i], format_name(test_ids[i]));
    }

    return 0;
}
```

(The numeric format IDs shown are illustrative teaching values, not necessarily MB-System's exact current registered format codes — always confirm the real, current table directly in `mb_format.c` when it matters.) Real MB-System format dispatch, incidentally, is table-driven through arrays of function pointers rather than a giant `switch` — a pattern Part 20 covers once function pointers themselves have been introduced. `switch` is nonetheless the right first tool for understanding the underlying *concept* of "dispatch by numeric code."

Every `case` needs an explicit `break;`, or execution silently "falls through" into the next `case`'s code — occasionally intentional, but far more often a forgotten-`break` bug. `return`, as used above, also exits the function immediately and has the same effect as `break` here, since nothing after the `switch` would otherwise execute anyway.

---

## Part 8 — Loops: Repeating Work

### 8.1 `for`

The workhorse loop for "repeat N times" or "iterate over an array of known length" — describing the overwhelming majority of MB-System's per-beam and per-ping processing loops.

```c
#include <stdio.h>

int main(void) {
    float depths_m[8] = {12.1f, 12.4f, 11.9f, 250.0f, 12.6f, 12.2f, -1.0f, 12.5f};
    int   nbeams = 8;

    double sum = 0.0;
    int    good_count = 0;

    for (int i = 0; i < nbeams; i++) {
        if (depths_m[i] <= 0.0f || depths_m[i] > 200.0f) {
            printf("Beam %d flagged as bad: %.2f m\n", i, depths_m[i]);
            continue;
        }
        sum += depths_m[i];
        good_count++;
    }

    if (good_count > 0) {
        printf("Average of %d good beams: %.3f m\n", good_count, sum / good_count);
    } else {
        printf("No good beams in this ping.\n");
    }

    return 0;
}
```

A `for` loop's header has three parts, separated by semicolons: an initializer (`int i = 0`, run once, before the loop starts), a condition (`i < nbeams`, checked before every iteration — the loop stops as soon as this becomes false), and an increment (`i++`, run after every iteration's body). This example is a close analogue of the per-ping quality summary logic real `mbinfo` produces for an entire swath file, and closely resembles the beam-loop pattern inside `mbclean`'s and `mbareaclean`'s outlier-flagging code. `continue` skips the remainder of the current iteration's body and jumps straight to the next iteration's condition check — a natural fit for "skip bad data, keep going," a pattern that recurs constantly in survey-data quality-control code.

### 8.2 `while`

`while` loops when you do not know the iteration count in advance — exactly the shape needed for "keep reading pings until end-of-file," which is precisely how every MB-System command-line tool's main processing loop is structured:

```c
#include <stdio.h>

int main(void) {
    int ping_number = 0;
    int more_data = 1;   /* stand-in for MBIO's real end-of-file/error signaling */

    while (more_data) {
        printf("Processing ping %d\n", ping_number);
        ping_number++;

        if (ping_number >= 5) {
            more_data = 0;   /* simulate reaching end of file after 5 pings */
        }
    }

    printf("Finished. Processed %d pings.\n", ping_number);
    return 0;
}
```

We replace this simulated `more_data` flag with a real call to `mb_read()` and real MBIO error-code checking in Part 19, once structs and pointers make that call's signature fully legible. For now, absorb the shape: **read, check, process, in a loop, until end-of-file or error** is the single most repeated control-flow pattern across the entire MB-System utilities directory.

### 8.3 `do-while`

A `while` loop checks its condition *before* the first iteration, so its body may run zero times. A `do-while` loop checks *after*, guaranteeing at least one iteration:

```c
int attempts = 0;
int success;

do {
    attempts++;
    success = try_open_file();   /* hypothetical function; always try at least once */
} while (!success && attempts < 3);
```

### 8.4 `break`

`break` exits the innermost enclosing loop (or `switch`) immediately, regardless of the loop's own condition:

```c
for (int i = 0; i < nbeams; i++) {
    if (depths_m[i] < -1000.0f) {
        printf("Corrupt data detected at beam %d, aborting ping processing.\n", i);
        break;
    }
    /* normal processing */
}
```

---

## Part 9 — Functions

### 9.1 Why Functions Exist

A function is a named, reusable block of code that performs one task, optionally taking inputs (**parameters**) and optionally producing an output (a **return value**). Functions let you write a calculation once and use it many times, give a complex operation a clear name (turning "multiply this by cosine of that, in radians" into `compute_depth_from_beam`), and organize a large program into manageable, independently testable pieces — exactly how a codebase the size of MB-System, with hundreds of thousands of lines, stays comprehensible at all.

### 9.2 Declaring, Defining, and Calling

```c
float compute_swath_width(float depth_m, float max_beam_angle_deg) {
    double angle_rad = max_beam_angle_deg * 3.14159265358979 / 180.0;
    return (float)(2.0 * depth_m * tan(angle_rad));
}
```

`float` is the return type; `compute_swath_width` is the name; `(float depth_m, float max_beam_angle_deg)` is the **parameter list** — the inputs this function expects, each with its own name and type, used inside the function body exactly like local variables. Calling it:

```c
float width = compute_swath_width(1500.0f, 60.0f);
printf("Estimated swath width: %.1f m\n", width);
```

### 9.3 Function Prototypes and Header Files

If a function is called before the point in a file where it is fully defined — or, far more commonly, defined in an entirely different `.c` file — the compiler needs to see its **prototype**: the signature alone, without a body, ending in a semicolon. Prototypes are almost always placed in a header file (`.h`) so multiple `.c` files can share them:

```c
/* swath.h */
#ifndef SWATH_H
#define SWATH_H

float compute_swath_width(float depth_m, float max_beam_angle_deg);

#endif
```

```c
/* swath.c */
#include <math.h>
#include "swath.h"

float compute_swath_width(float depth_m, float max_beam_angle_deg) {
    double angle_rad = max_beam_angle_deg * M_PI / 180.0;
    return (float)(2.0 * depth_m * tan(angle_rad));
}
```

The `#ifndef SWATH_H` / `#define SWATH_H` / `#endif` triplet is called a **header guard** (fully explained in Part 16); it prevents errors if this header is accidentally `#include`d more than once within a single compiled file — exactly the pattern used by essentially every header throughout `src/mbio/` and `src/mbaux/`.

### 9.4 Scope: Where a Variable "Exists"

A variable declared inside a function body (a **local variable**) exists only from its declaration until the closing `}` of the block it is declared in, and is completely inaccessible from any other function. A variable declared outside any function (a **global variable**) exists for the entire program's lifetime and is visible to every function in the same file (and, if declared `extern` in a header, to other files too). MB-System, like most large, careful C codebases, uses global variables sparingly and deliberately — mostly for a small number of genuinely program-wide settings — because global state makes a program's behavior harder to reason about: any function, anywhere, could be reading or modifying it.

```c
int global_verbose = 0;   /* global: visible to every function below, in this file */

void report(const char *message) {
    if (global_verbose) {
        printf("[verbose] %s\n", message);
    }
}

int main(void) {
    int local_count = 5;   /* local: exists only inside main */
    global_verbose = 1;
    report("starting processing");
    return 0;
}
```

### 9.5 MB-System's Error-Handling Convention, Introduced

This is one of the single most important patterns in the entire codebase. C has no exceptions, and relying purely on a function's return value to carry both "did it succeed" and "what exactly is the answer" runs out of room quickly. MBIO functions almost universally follow this shape instead:

```c
int mb_read_init(int verbose, char *file, int format, /* ... more args ... */ int *error);
```

The actual return value is a **status code** — `MB_SUCCESS` or `MB_FAILURE` — telling the caller only whether the call succeeded. Detailed information about *what* went wrong is written into an `int` variable that the *caller* owns, passed **by pointer** (`int *error`), so the function can modify the caller's own variable directly. A small, original teaching version, exercising exactly this pattern:

```c
#include <stdio.h>

#define MY_SUCCESS 0
#define MY_FAILURE 1
#define MY_ERROR_NONE       0
#define MY_ERROR_BAD_DEPTH 10
#define MY_ERROR_BAD_ANGLE 11

int validate_beam(double depth_m, double angle_deg, int *error) {
    *error = MY_ERROR_NONE;

    if (depth_m <= 0.0) {
        *error = MY_ERROR_BAD_DEPTH;
        return MY_FAILURE;
    }
    if (angle_deg < -90.0 || angle_deg > 90.0) {
        *error = MY_ERROR_BAD_ANGLE;
        return MY_FAILURE;
    }
    return MY_SUCCESS;
}

int main(void) {
    int error = MY_ERROR_NONE;
    int status = validate_beam(-5.0, 12.0, &error);

    if (status == MY_FAILURE) {
        printf("Validation failed, error code %d\n", error);
    } else {
        printf("Beam is valid.\n");
    }

    return 0;
}
```

`&error` passes the *address* of `error`, not its current value, so `validate_beam` can write into the caller's actual variable. Part 11 explains precisely why this works, at the level of memory addresses; for now, memorize the *shape*: **status code as return value, detailed error delivered through a pointer parameter.** You will see this exact shape in nearly every MBIO function you ever call.

### 9.6 Returning Multiple Values Through Pointers

A `return` statement can only hand back one value directly, so C functions that need to produce several results at once deliver the extras through pointer parameters, exactly as with the error convention above:

```c
void beam_to_depth_and_across(double angle_deg, double slant_range_m,
                               double *depth_m, double *across_track_m) {
    double angle_rad = angle_deg * 3.14159265358979 / 180.0;
    *depth_m        = slant_range_m * cos(angle_rad);
    *across_track_m = slant_range_m * sin(angle_rad);
}
```

```c
double depth, across;
beam_to_depth_and_across(32.5, 45.2, &depth, &across);
printf("depth=%.3f across=%.3f\n", depth, across);
```

### 9.7 Recursion

A function may call itself; this is called **recursion**, useful when a problem naturally breaks down into smaller versions of itself. Recursion appears far less often in MB-System's I/O-heavy code than iteration (loops) does, but it is a standard part of the language worth knowing:

```c
unsigned long factorial(unsigned int n) {
    if (n <= 1) {
        return 1;             /* base case: stops the recursion */
    }
    return n * factorial(n - 1);  /* recursive case: calls itself with a smaller input */
}
```

Every recursive function needs a **base case** that stops the recursion; without one, the function calls itself forever (or, in practice, until it exhausts the program's call stack and crashes).

---

## Part 10 — Arrays

### 10.1 What an Array Is

An array is a fixed-size, contiguous block of memory holding multiple values of the *same* type, accessed by a numeric **index** starting at 0:

```c
float depths_m[8];     /* room for exactly 8 float values, uninitialized */
depths_m[0] = 12.1f;
depths_m[1] = 12.4f;
/* ... */
depths_m[7] = 12.5f;
```

"Contiguous" means all 8 `float` values sit immediately next to each other in memory, in order, with no gaps — `depths_m[0]` occupies the first 4 bytes of the block, `depths_m[1]` the next 4 bytes, and so on. This layout is precisely why array indexing is extremely fast: the address of `depths_m[i]` is computed as simply `(base address of depths_m) + i * (size of one float)` — a single multiplication and addition, not a search.

### 10.2 Array Initialization

```c
float depths_m[8] = {12.1f, 12.4f, 11.9f, 250.0f, 12.6f, 12.2f, -1.0f, 12.5f};
int   counts[5]   = {0};   /* initializes ALL 5 elements to 0, a common idiom */
```

### 10.3 Indexing and Bounds

Valid indices for an array of size N run from `0` to `N-1`. **C performs no automatic bounds checking whatsoever.** Writing `depths_m[8]` on an 8-element array (valid indices 0–7) does not produce an error message — it silently reads or writes whatever memory happens to sit immediately past the array, corrupting unrelated data or crashing unpredictably, possibly much later in the program, far from the actual mistake. This class of bug — an **out-of-bounds array access** — is one of the most consequential categories of real-world C bugs, and MB-System's own changelogs record numerous fixes for exactly this kind of issue in ping- and beam-array handling code, underscoring how easy it is to get wrong even in mature, widely used software. Always be certain your loop bounds and index arithmetic match your array's actual declared size.

### 10.4 Arrays as Function Parameters

When you pass an array to a function, C does **not** copy the whole array — it passes a pointer to the array's first element (Part 11 explains exactly what this means). A practical, immediate consequence: the function has no built-in way to know how many elements the array actually holds, so array-processing functions almost always take an explicit length parameter alongside the array itself, which is exactly why MBIO functions that operate on beam arrays always take a beam-count parameter too:

```c
int count_flagged_beams(float depths[], int nbeams, float min_valid, float max_valid) {
    int flagged = 0;
    for (int i = 0; i < nbeams; i++) {
        if (depths[i] < min_valid || depths[i] > max_valid) {
            flagged++;
        }
    }
    return flagged;
}
```

```c
float depths_m[8] = {12.1f, 12.4f, 11.9f, 250.0f, 12.6f, 12.2f, -1.0f, 12.5f};
int bad = count_flagged_beams(depths_m, 8, 0.0f, 200.0f);
printf("%d flagged beams\n", bad);
```

### 10.5 Two-Dimensional Arrays

A two-dimensional array is, conceptually, an array of arrays — useful for grid-like data such as a bathymetric grid of depth values indexed by row and column:

```c
#define NROWS 3
#define NCOLS 4

float grid_m[NROWS][NCOLS] = {
    {10.1f, 10.3f, 10.5f, 10.7f},
    {10.2f, 10.4f, 10.6f, 10.8f},
    {10.0f, 10.2f, 10.4f, 10.6f}
};

for (int row = 0; row < NROWS; row++) {
    for (int col = 0; col < NCOLS; col++) {
        printf("%.1f ", grid_m[row][col]);
    }
    printf("\n");
}
```

Real MB-System gridding tools like `mbgrid` work with grids far larger than this fixed-size teaching example — typically many thousands of rows and columns — and therefore cannot use a compile-time-fixed 2D array like this at all, since its size must be known when the program is compiled, long before the actual grid dimensions (which depend on the survey area and chosen cell size) are known. Real gridding code instead allocates one large, single-dimensional block of memory at runtime, sized exactly to the data at hand, and computes each cell's position within that block manually — a technique made possible by dynamic memory allocation, covered fully in Part 14, and pointer arithmetic, covered in Part 11.

---

## Part 11 — Pointers

This is the single most important part of this primer for genuinely understanding MB-System's C code, and it deserves unhurried, careful attention.

### 11.1 What a Pointer Actually Is

Every variable, once declared, lives at some specific address in memory — a numeric location, just like a house number on a street. A **pointer** is simply a variable whose *value* is one of these addresses — it "points at" the location of another variable, rather than holding ordinary data like a number or character directly.

```c
int beam_count = 24;
int *pointer_to_beam_count = &beam_count;
```

`&beam_count` is the **address-of** operator applied to `beam_count`: it evaluates to the memory address where `beam_count` lives, not to `beam_count`'s value (`24`). `int *pointer_to_beam_count` declares a variable named `pointer_to_beam_count` whose type is "pointer to `int`" — meaning it is meant to hold the address of some `int` variable somewhere in memory. After this assignment, `pointer_to_beam_count` holds `beam_count`'s address; it does *not* hold `24`.

### 11.2 Dereferencing: Getting the Value a Pointer Points At

The `*` operator, when applied to an *existing pointer variable* (as opposed to appearing in a type declaration), means "the value stored at the address this pointer holds" — this is called **dereferencing**:

```c
int beam_count = 24;
int *ptr = &beam_count;

printf("%d\n", *ptr);   /* prints 24 -- the value beam_count currently holds */

*ptr = 30;              /* writes 30 into whatever address ptr holds -- i.e., into beam_count itself */
printf("%d\n", beam_count);  /* prints 30 -- beam_count has genuinely changed */
```

This is the entire mechanism behind the error-handling and multiple-return-value patterns from Part 9: when a function receives a pointer parameter and dereferences it to write a value, it is reaching *back out* of its own local scope and modifying a variable that belongs to its caller. Nothing magical is happening — it is exactly the mechanism just shown, `*ptr = 30;`, applied inside a function body instead of directly in `main`.

### 11.3 Why This Matters: Passing Large Data Without Copying It

Consider a function meant to process an entire array of, say, 400 beam depth values from one multibeam ping. If C passed arrays "by value" (copying the entire array's contents every time it is passed to a function), every single function call touching ping data would silently copy hundreds of floats, repeatedly, throughout a program's execution — a serious, needless performance cost when processing files with hundreds of thousands of pings. C avoids this entirely: as Section 10.4 already showed, passing an array to a function passes only a pointer to its first element — a single address, typically 8 bytes on a 64-bit system, regardless of whether the array behind it holds 8 elements or 8,000. This is *the* foundational reason MBIO can pass entire beam arrays between its generic API layer and format-specific reader functions efficiently: no matter how many beams a ping contains, passing that ping's depth array anywhere in the program costs exactly one pointer's worth of data to hand over.

### 11.4 Pointers and Arrays Are Deeply Related

An array's name, used in most expressions, automatically evaluates to a pointer to its first element. This is why the following two ways of writing "the third element of `depths`" are exactly equivalent:

```c
float depths[8] = { /* ... */ };

float value1 = depths[2];       /* array indexing syntax */
float value2 = *(depths + 2);   /* pointer arithmetic syntax -- identical meaning */
```

`depths + 2` means "the address 2 `float`-sized steps past `depths`'s first element" — the compiler automatically scales the `+ 2` by `sizeof(float)` (4 bytes), because it knows `depths` is an array of `float`. `*(depths + 2)` then dereferences that computed address, retrieving the value stored there. **This is exactly what `depths[2]` means, under the hood — array indexing is defined in the C language specification purely in terms of pointer arithmetic like this.** You will rarely need to write `*(depths + 2)` explicitly in your own code — `depths[2]` is clearer and you should always prefer it — but recognizing this equivalence is essential for reading real MBIO source, which sometimes does use raw pointer-arithmetic style when walking through a buffer of raw bytes read directly off disk, one record at a time, where the exact byte-width of each step matters more visibly than array-index notation would make clear.

### 11.5 Pointers to Structures (Preview)

Part 13 covers structures fully, but one pointer-related detail belongs here because it is used everywhere: given a pointer to a structure, you access its members using `->` instead of `.`:

```c
struct Point { double x, y; };

struct Point p1 = {1.0, 2.0};
struct Point *ptr = &p1;

printf("%f\n", p1.x);    /* direct access: dot */
printf("%f\n", ptr->x);  /* access through a pointer: arrow */
```

`ptr->x` is exact shorthand for `(*ptr).x` — "dereference the pointer to get the structure, then access its `.x` member" — written as a single, more readable operator. Since MBIO passes its central bookkeeping structure (`mb_io_struct`, covered in Part 18) around almost exclusively via pointers, `->` is one of the single most frequently occurring pieces of syntax in the entire MBIO codebase, and you should expect to type it constantly.

### 11.6 NULL Pointers

A pointer that does not currently point at any valid variable should be set to `NULL` (a special constant, effectively address zero, defined in several standard headers), and should always be checked before being dereferenced:

```c
int *ptr = NULL;

if (ptr != NULL) {
    printf("%d\n", *ptr);
} else {
    printf("ptr is not pointing at anything valid.\n");
}
```

Dereferencing a `NULL` pointer (`*ptr` when `ptr == NULL`) is a serious runtime error that immediately crashes the program (a "segmentation fault" or "null pointer dereference"). MBIO functions that allocate memory and return a pointer to it (Part 14) very commonly signal an allocation failure by returning `NULL`, which is exactly why so much real MBIO code contains a `if (something == NULL) { ... handle error ... }` check immediately after essentially every allocation.

### 11.7 Pointer Arithmetic and Its Dangers

Because pointer arithmetic (Section 11.4) computes new addresses by simple offset math, it is entirely possible to compute an address that no longer points at anything valid — one step past the end of an array, for instance — and C will not stop you from dereferencing it. This is the pointer-flavored version of the array out-of-bounds problem from Section 10.3, and the two are, at the memory level, literally the same underlying mistake. The practical rule, which every subsequent part of this primer will reinforce: **whenever you compute a pointer via arithmetic or receive one as a function parameter, know exactly how far past its starting point it is safe to go, and never go further.**

---

## Part 12 — Strings

### 12.1 What a String Is in C

C has no dedicated built-in string type. A "string" is, by convention, simply an array of `char` values, ending with a special **null terminator** byte, written `'\0'` (the character with numeric value 0 — not to be confused with the character `'0'`, which has a different, nonzero numeric value):

```c
char sensor_name[6] = {'E', 'M', '7', '1', '0', '\0'};   /* the hard way */
char sensor_name2[6] = "EM710";                          /* the normal way -- identical result */
```

`"EM710"` is called a **string literal**; the compiler automatically appends the `'\0'` terminator for you, meaning a literal of N visible characters needs an array of at least N+1 `char`s to hold it. Every C function that works with strings — `printf`'s `%s`, `strlen`, `strcmp`, and so on — relies entirely on finding that `'\0'` byte to know where the string ends; there is no separately stored "length" the way many higher-level languages provide automatically.

### 12.2 Common String Functions (`<string.h>`)

| Function | Purpose |
|---|---|
| `strlen(s)` | returns the length of `s`, *not* counting the `'\0'` |
| `strcpy(dest, src)` | copies `src` into `dest`, including the terminator |
| `strncpy(dest, src, n)` | copies at most `n` bytes — safer against overflow |
| `strcmp(a, b)` | returns 0 if `a` and `b` are identical strings |
| `strcat(dest, src)` | appends `src` onto the end of `dest` |
| `strchr(s, c)` | finds the first occurrence of character `c` in `s` |
| `strstr(a, b)` | finds the first occurrence of substring `b` in `a` |

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char format_string[32] = "KEMKMALL";

    printf("Length: %zu\n", strlen(format_string));   /* %zu for size_t, strlen's return type */

    if (strcmp(format_string, "KEMKMALL") == 0) {
        printf("This is a Kongsberg KMALL-family file.\n");
    }

    return 0;
}
```

Note `strcmp` returning `0` for equal strings, not `1` — a common early trap, since `0` reads intuitively as "false"/"no" in most other contexts, but here means "no difference found," i.e., a match.

### 12.3 Buffer Overflows: Why String Handling Is Dangerous in C

Because a fixed-size `char` array has a definite capacity, and C provides no automatic protection against writing past that capacity, copying a string that is longer than its destination buffer silently overwrites whatever memory comes immediately after the buffer — a **buffer overflow**, historically among the most serious and exploitable classes of bugs in all of C/C++ software. `strcpy`, in particular, performs no length check whatsoever:

```c
char short_buffer[8];
strcpy(short_buffer, "This string is much too long for an 8-byte buffer");  /* DANGEROUS */
```

The safer alternative, `strncpy`, takes an explicit maximum length, though it has its own subtlety: if the source string is *at least* as long as the given limit, `strncpy` does **not** automatically add a `'\0'` terminator, so you must add one yourself:

```c
char safer_buffer[8];
strncpy(safer_buffer, "This string is much too long", sizeof(safer_buffer) - 1);
safer_buffer[sizeof(safer_buffer) - 1] = '\0';   /* guarantee termination */
```

MB-System's own bundled fixed-size buffers for filenames, comments, and format names (Section 4.7's `sensor_name[32]`, for example) rely on exactly this kind of careful, size-aware copying throughout the codebase, and reviewing unfamiliar string-handling code for a missing length check or missing terminator is one of the most valuable habits you can build as you start reading real source.

---

## Part 13 — Structures

### 13.1 What a Structure Is

A `struct` groups several related variables — possibly of different types — together under one name, so they can be treated as a single unit:

```c
struct Beam {
    int    beam_number;
    double angle_deg;
    double range_m;
    float  depth_m;
    float  across_track_m;
    int    flag;    /* 0 = good, nonzero = flagged/bad, by convention */
};
```

Using it:

```c
struct Beam b1;
b1.beam_number    = 12;
b1.angle_deg      = 32.5;
b1.range_m        = 45.2;
b1.depth_m        = 38.1f;
b1.across_track_m = 24.3f;
b1.flag           = 0;

printf("Beam %d: depth %.2f m, flag %d\n", b1.beam_number, b1.depth_m, b1.flag);
```

`.` accesses a member of a structure variable directly; recall from Section 11.5 that `->` does the same thing through a pointer to a structure.

### 13.2 `typedef`: Giving a Structure Type a Shorter Name

Writing `struct Beam` every time is verbose. `typedef` creates an alias:

```c
typedef struct Beam {
    int    beam_number;
    double angle_deg;
    double range_m;
    float  depth_m;
    float  across_track_m;
    int    flag;
} Beam;
```

Now `Beam b1;` alone is valid — no `struct` keyword needed. MB-System's own headers use exactly this pattern extensively, defining structure types with names ending, by convention, in `_struct` and then a matching `typedef` name.

### 13.3 Structures Containing Arrays: A Simplified Ping

```c
#define MAX_BEAMS 400

typedef struct {
    int    ping_number;
    double time_stamp;         /* seconds since epoch */
    double latitude_deg;
    double longitude_deg;
    int    nbeams;
    float  depth_m[MAX_BEAMS];
    float  across_track_m[MAX_BEAMS];
    int    flag[MAX_BEAMS];
} SimplePing;
```

This is a deliberately simplified, book-original structure — nowhere near as large or format-general as MBIO's real internal ping structures, which additionally carry heading, roll, pitch, heave, sound speed, backscatter amplitude, sidescan samples, and format-specific raw fields — but it captures the essential *shape* of the real thing closely enough to practice with:

```c
SimplePing ping;
ping.ping_number = 1;
ping.nbeams = 3;
ping.depth_m[0] = 12.1f;
ping.depth_m[1] = 12.4f;
ping.depth_m[2] = 11.9f;

double sum = 0.0;
for (int i = 0; i < ping.nbeams; i++) {
    sum += ping.depth_m[i];
}
printf("Average depth: %.3f m\n", sum / ping.nbeams);
```

A fixed-size array member like `depth_m[MAX_BEAMS]` above works fine for teaching purposes, but wastes memory when most pings carry far fewer beams than `MAX_BEAMS`, and hard-fails outright for any sonar producing more beams than that fixed limit — precisely the problem real MBIO code solves with dynamically allocated arrays instead, covered next in Part 14.

### 13.4 Nested Structures and MB-System's `mb_io_struct` (Preview)

Real MBIO code centers on a large structure, conventionally named `mb_io_struct`, that every open swath file has exactly one instance of, holding the file handle, current format information, buffers for the current ping's data, and pointers to format-specific data (covered fully once dynamic memory and file I/O are both available, in Part 18). Structures can contain other structures, and very commonly contain *pointers* to other structures or to dynamically allocated arrays, rather than the arrays themselves — exactly the technique that lets one generic `mb_io_struct` accommodate wildly different beam counts and record layouts across dozens of supported sonar formats, without wasting memory on a one-size-fits-all fixed maximum.

---

## Part 14 — Dynamic Memory Allocation

### 14.1 Why Fixed-Size Arrays Are Not Enough

Section 13.3's `depth_m[MAX_BEAMS]` array has its size fixed at *compile time* — chosen once, when you write the code, before you know anything about the actual files your program will eventually process. Real multibeam files vary enormously in beam count across sonar models and modes, and a program that must handle *any* of them needs to decide how much memory to use only once it actually knows, at *run time*, how many beams a given ping or file actually has. This is exactly the problem **dynamic memory allocation** solves.

### 14.2 `malloc` and `free`

`malloc` (from `<stdlib.h>`) requests a block of memory of a given size, at runtime, and returns a pointer to it (or `NULL` if the request could not be satisfied — always check):

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int nbeams = 256;   /* imagine this came from reading a real file's header */

    float *depths_m = (float *)malloc(nbeams * sizeof(float));

    if (depths_m == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    for (int i = 0; i < nbeams; i++) {
        depths_m[i] = 0.0f;   /* initialize -- malloc does NOT zero memory for you */
    }

    depths_m[10] = 45.7f;
    printf("Beam 10 depth: %.2f m\n", depths_m[10]);

    free(depths_m);   /* release the memory back to the system when done */
    depths_m = NULL;   /* good practice: avoid an accidental later use of a freed pointer */

    return 0;
}
```

`sizeof(float)` asks the compiler for the exact byte size of a `float` on the current platform, rather than hard-coding "4," which keeps the code correct even on an unusual platform where sizes differ — a habit used throughout MB-System's own allocation code. `malloc`'s return type is technically `void *` (a generic pointer type meaning "an address, with no specified type of data at it"), which is why we cast it to `float *` before use — modern C actually allows the assignment without an explicit cast, but writing the cast makes the intended type visible to a human reader at the allocation site, a stylistic choice you will see both with and without the cast in real code.

**Critically, `malloc` does not initialize the memory it hands you** — unlike a fixed-size array declared with `= {0}`, freshly `malloc`'d memory contains whatever bits happened to be there before, and must be explicitly initialized (as the loop above does) before being trusted.

### 14.3 `calloc`: Allocate and Zero in One Step

`calloc(count, size)` allocates room for `count` elements of `size` bytes each, and additionally guarantees the entire block starts zeroed:

```c
float *depths_m = (float *)calloc(nbeams, sizeof(float));
```

This is often preferable to `malloc` immediately followed by a manual zeroing loop, both for clarity and because it avoids the (admittedly easy to make correctly) but ever-present risk of forgetting the zeroing step.

### 14.4 `realloc`: Growing or Shrinking an Existing Allocation

If you discover partway through processing that you need a larger (or smaller) block than you originally allocated, `realloc` resizes an existing allocation, preserving its existing content up to the smaller of the old and new sizes:

```c
depths_m = (float *)realloc(depths_m, new_nbeams * sizeof(float));
if (depths_m == NULL) {
    fprintf(stderr, "Reallocation failed.\n");
    return 1;
}
```

`realloc` may return a *different* address than the block previously occupied (moving the data internally if necessary), which is exactly why you must always reassign the result back into your pointer variable, as shown, rather than assuming the memory stayed in place.

### 14.5 Memory Leaks and Double Frees

Every successful `malloc`/`calloc`/`realloc` call obligates your program to eventually call `free` on that same pointer exactly once. Forgetting to `free` memory you no longer need is a **memory leak** — harmless for a short-lived program, but a serious problem for a long-running one, or one that processes many files/pings in a loop and leaks a little memory on each iteration, eventually exhausting available memory entirely. Calling `free` twice on the same pointer (a **double free**), or using a pointer after it has been freed (a **use-after-free**), are both serious errors that corrupt the program's memory-management bookkeeping and can crash the program or, worse, silently corrupt unrelated data. MB-System's own changelogs record fixes for exactly these classes of bugs — memory leaks in navigation and route-handling code, and a double-free arising from two structures accidentally aliasing (sharing) the same underlying allocated array after a deletion operation failed to clear a stale pointer. Recognizing this exact failure shape — a deletion or resize operation that shifts or removes entries without correctly freeing and clearing what was removed — is a genuinely valuable, transferable skill once you start reading real MBIO and mbview code with allocation and deallocation logic in it.

### 14.6 Why This Matters for MB-System Specifically

Every MBIO reader function that opens a new format must, at some point, allocate the format-specific "store" structure sized appropriately for that file, and every corresponding close function must free it again. Getting this exactly right — allocating enough space, initializing it correctly, and freeing it exactly once, in every code path including error paths — is a substantial fraction of what a correct MBIO format driver actually has to do, and it is why Part 14's concepts are treated as a full, dedicated part of this primer rather than a brief aside.

---

## Part 15 — File Input and Output

### 15.1 Opening and Closing a File

```c
#include <stdio.h>

int main(void) {
    FILE *fp = fopen("example.txt", "r");   /* "r" = open for reading, text mode */

    if (fp == NULL) {
        fprintf(stderr, "Could not open file.\n");
        return 1;
    }

    /* ... use fp here ... */

    fclose(fp);
    return 0;
}
```

`FILE *` is an opaque pointer type representing an open file; you never need to know (or should try to inspect) what its internals actually contain — you only ever pass it to the standard library's `f*` functions. The second argument to `fopen` is the **mode** string: `"r"` read, `"w"` write (creating the file, or erasing its existing contents), `"a"` append, and, crucially for binary sonar data, `"rb"`/`"wb"` for binary read/write.

### 15.2 Text Mode vs. Binary Mode

This distinction matters enormously for MB-System work. **Text mode** is intended for human-readable content and, on some platforms (notably Windows), silently translates line-ending bytes during reading/writing. **Binary mode** (`"rb"`, `"wb"`) transfers bytes exactly as they exist on disk, with no translation whatsoever — the only correct choice for reading a manufacturer's raw binary multibeam record format, where every byte's exact value is meaningful data, not incidental text formatting. Always use binary mode for swath sonar files.

### 15.3 Reading and Writing Raw Binary Data

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    FILE *fp = fopen("raw_pings.bin", "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file.\n");
        return 1;
    }

    int32_t ping_number;
    double  time_stamp;

    size_t n_read = fread(&ping_number, sizeof(ping_number), 1, fp);
    n_read += fread(&time_stamp, sizeof(time_stamp), 1, fp);

    if (n_read != 2) {
        fprintf(stderr, "Unexpected end of file or read error.\n");
        fclose(fp);
        return 1;
    }

    printf("Ping %d at time %.3f\n", ping_number, time_stamp);

    fclose(fp);
    return 0;
}
```

`fread(pointer, size_of_one_item, number_of_items, file)` reads raw bytes directly into the memory `pointer` points at, exactly as they appear on disk — no parsing, no interpretation, just a byte-for-byte copy of `size_of_one_item * number_of_items` bytes. This is the fundamental mechanism underlying every MBIO format reader: reading a fixed-size binary header this way, examining a record-type or record-length field within it, and then, based on that, reading the appropriate number of further bytes for that specific record type — precisely the pattern you already recognize from working with KMALL and GSF files' own record structures. `fread`'s return value — the number of *items* (not bytes) actually read — must always be checked, since it will be smaller than requested at end-of-file or on a read error, exactly the situation the check above guards against.

`fwrite(pointer, size_of_one_item, number_of_items, file)` is `fread`'s mirror image, writing raw bytes out.

### 15.4 Byte Order (Endianness): A Critical Real-World Complication

Different computer architectures store the bytes of a multi-byte number in different orders. A 4-byte integer can be stored **big-endian** (most significant byte first) or **little-endian** (least significant byte first, the near-universal convention on x86/x86-64 and Apple Silicon systems today). Many sonar manufacturers' binary formats specify a fixed byte order in their format specification, which may or may not match the byte order of the machine currently running your program. Reading a big-endian-specified integer field naively on a little-endian machine, without swapping the byte order first, silently produces a completely wrong numeric value — not a crash, not an error, just a wrong answer that can look superficially plausible. MB-System's format-reading code contains byte-swapping logic precisely to handle this, and it is one of the most important, easy-to-overlook details when writing or debugging any binary-format reader, including for KMALL and GSF files, which you already work with directly.

### 15.5 Checking for End-of-File and Errors

```c
int32_t value;
size_t n = fread(&value, sizeof(value), 1, fp);

if (n < 1) {
    if (feof(fp)) {
        printf("Reached end of file normally.\n");
    } else if (ferror(fp)) {
        fprintf(stderr, "A read error occurred.\n");
    }
}
```

`feof` and `ferror` let you distinguish "there was simply no more data" (the normal, expected way a file-reading loop ends) from "something actually went wrong" (a genuine error condition) — a distinction MBIO's own error codes make explicit via a dedicated `MB_ERROR_EOF` constant alongside its many other, genuinely erroneous `MB_ERROR_*` codes.

---

## Part 16 — The Preprocessor

### 16.1 What the Preprocessor Does

Before actual compilation begins, a separate stage called the **preprocessor** performs purely textual substitutions on your source code, based on lines beginning with `#`. Understanding it fully resolves several things left as "we'll explain this later" earlier in this primer.

### 16.2 `#include`

```c
#include <stdio.h>   /* search the compiler's standard system include paths */
#include "swath.h"   /* search first in the current/project directory */
```

The preprocessor literally copies the *entire contents* of the named file, verbatim, in place of the `#include` line, before compilation proper even begins. This is why declaring a function's prototype in a header and `#include`-ing that header in multiple `.c` files works: each `.c` file ends up, after preprocessing, containing its own literal copy of that prototype text.

### 16.3 `#define`: Macros

```c
#define MAX_BEAMS 400
#define SOUND_SPEED_DEFAULT 1500.0
```

Every later occurrence of `MAX_BEAMS` in the source text is replaced, purely textually, with `400` before compilation. Unlike a `const` variable (Section 4.5), a `#define` macro has no type and consumes no memory at runtime — it is gone entirely by the time actual compilation happens, having been replaced by literal text.

Macros can also take parameters, though with an important caveat:

```c
#define SQUARE(x) ((x) * (x))
```

The extra parentheses around `x` and around the whole expression are not decorative — without them, `SQUARE(a + b)` would textually expand to `a + b * a + b`, which, due to operator precedence, computes something completely different from the intended `(a + b) * (a + b)`. Always parenthesize both individual macro parameters and the macro's overall expansion, exactly as shown. MB-System's own historical source (recall the SBSIOSWB bug mentioned in Part 2, caused by exactly a poorly parenthesized rounding macro dating to 1992) is a real, documented example of precisely this class of mistake surviving undetected for decades until it happened to surface on a newer compiler/OS combination — a genuinely instructive cautionary tale about the preprocessor's purely textual, precedence-blind nature.

### 16.4 Header Guards, Explained Fully

```c
#ifndef SWATH_H
#define SWATH_H

/* ... declarations ... */

#endif
```

`#ifndef SWATH_H` means "if the macro `SWATH_H` is *not* currently defined, include everything up to the matching `#endif`." The very next line defines `SWATH_H`. The first time this header is `#include`d anywhere within a single compiled file, `SWATH_H` is not yet defined, so its contents are included, and `SWATH_H` becomes defined as a side effect. If the same header is accidentally `#include`d again later in the same file (directly, or indirectly through another header that also includes it), `SWATH_H` is now already defined, so the `#ifndef` check fails and the entire contents are skipped — preventing the compiler from seeing the same declarations twice, which would otherwise be a compile error.

### 16.5 Conditional Compilation

`#ifdef`/`#ifndef`/`#else`/`#endif` can also gate entire blocks of code based on whether some macro is defined — used throughout MB-System to handle platform differences (Windows vs. Unix-like systems), optional features (whether GMT or Qt support was detected at configure time), and debug-only diagnostic code:

```c
#ifdef DEBUG_BUILD
    printf("Debug: about to read %d beams\n", nbeams);
#endif
```

If `DEBUG_BUILD` is not defined when the file is compiled, the preprocessor removes that `printf` call entirely — it does not even exist in the compiled program, with zero runtime cost, as opposed to an `if` statement, which is checked at runtime, however cheaply.

---

## Part 17 — Multi-File Programs and the CMake Build System

### 17.1 Splitting Code Across Files, Reviewed

```c
/* swath.h */
#ifndef SWATH_H
#define SWATH_H
float compute_swath_width(float depth_m, float max_beam_angle_deg);
#endif
```

```c
/* swath.c */
#include <math.h>
#include "swath.h"
float compute_swath_width(float depth_m, float max_beam_angle_deg) {
    double angle_rad = max_beam_angle_deg * M_PI / 180.0;
    return (float)(2.0 * depth_m * tan(angle_rad));
}
```

```c
/* main.c */
#include <stdio.h>
#include "swath.h"
int main(void) {
    float width = compute_swath_width(1500.0f, 60.0f);
    printf("Estimated swath width: %.1f m\n", width);
    return 0;
}
```

### 17.2 Compiling by Hand

```sh
gcc -Wall -Wextra -g -c swath.c -o swath.o
gcc -Wall -Wextra -g -c main.c  -o main.o
gcc swath.o main.o -o mbswath -lm
```

`-c` means "compile only, produce an object file, do not link." The final command, given object files rather than source, performs only the **link** step, resolving `main.o`'s reference to `compute_swath_width` against its actual definition in `swath.o`. `-Wall -Wextra` enable a wide set of compiler warnings; compiling with these on at all times is one of the single highest-value habits in this entire primer.

### 17.3 A CMakeLists.txt for This Small Example

Rather than hand-writing a traditional `Makefile` (as a legacy Autotools-based project would), we describe this small multi-file program using CMake, matching exactly the style you will find throughout the real MB-System source tree:

```cmake
cmake_minimum_required(VERSION 3.16)
project(mbswath_example C)

add_executable(mbswath main.c swath.c)
target_link_libraries(mbswath m)   # links the math library, -lm
```

Build it exactly as you built MB-System itself in Part 2:

```sh
mkdir build && cd build
cmake ..
cmake --build .
./mbswath
```

`add_executable(mbswath main.c swath.c)` is the CMake equivalent of the `_SOURCES` list you would see in an old Autotools `Makefile.am`: it declares an executable target named `mbswath`, and lists exactly which `.c` files compile into it. `target_link_libraries(mbswath m)` links against the math library (CMake's convention omits the `-l` prefix and the leading `lib`; `m` here means `libm`).

### 17.4 Reading and Extending a Real MB-System `CMakeLists.txt`

A simplified excerpt in the spirit of what you will find in `src/mbio/CMakeLists.txt` looks roughly like this:

```cmake
add_library(mbio
    mb_read.c
    mb_write.c
    mb_format.c
    mbr_em710.c
    mbr_kemkmall.c
    mbsys_kmbes.c
    # ... many more entries ...
)

target_include_directories(mbio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mbio PUBLIC ${NETCDF_LIBRARIES} m)
```

`add_library(mbio ...)` is the crucial line for you as a future contributor: it is the literal, explicit list of `.c` files compiled into the `mbio` library target. **If you add a brand-new `.c` file to `src/mbio/` — for instance, a new format reader — and do not add its name to this list, CMake will never compile it, no matter how correct the code inside it is.** This is one of the single most common "why isn't my new code doing anything" mistakes for newcomers to any CMake-based C project, and now you know exactly why it happens and how to fix it: open the relevant `CMakeLists.txt`, add your filename to the appropriate `add_library` or `add_executable` call, save, and re-run `cmake --build .` from your existing `build/` directory — CMake automatically detects the changed `CMakeLists.txt` and regenerates its build files before compiling, with no separate "reconfigure" step required on your part for this kind of change.

### 17.5 Out-of-Source Builds and Rebuilding After Pulling Upstream Changes

Because all generated build artifacts live in the separate `build/` directory (Section 2.4), pulling new upstream commits into your source checkout never risks conflicting with any generated file. After a `git pull`, simply re-run `cmake --build .` from inside your existing `build/` directory; CMake detects which `CMakeLists.txt` files or source files changed and rebuilds only what is actually necessary — the same efficient, incremental-rebuild principle a traditional `Makefile`'s dependency tracking provides, just generated and managed automatically by CMake rather than hand-written.

---

## Part 18 — MB-System's Core Conventions in Depth

### 18.1 The Verbose Level Convention

Nearly every MBIO function's first parameter is `int verbose` — a numeric level (typically 0 through 2 or higher) controlling how much diagnostic output the function prints to standard error as it runs. `0` means silent; increasing values print progressively more detail. This convention lets every command-line tool expose a single, consistent `-V` flag that, when repeated or given a numeric argument, turns up the diagnostic detail from the *entire* call chain beneath it, all the way down into format-specific reader functions, without each individual function needing its own separate debug-flag design.

### 18.2 The Status/Error Convention, in Full

Building on Section 9.5, essentially every MBIO function follows this exact shape:

```c
int mb_read_init(int verbose,
                  char *file,
                  int  *format,
                  int  *pings,
                  double *btime_d,
                  double *etime_d,
                  double *speedmin,
                  double *timegap,
                  void **mbio_ptr,
                  double *btime_d_out,
                  double *etime_d_out,
                  int  *beams_bath,
                  int  *beams_amp,
                  int  *pixels_ss,
                  int  *error);
```

(This signature is simplified and illustrative of the *shape* of a real MBIO initialization call, not a byte-exact reproduction of the current function prototype — always check the actual current header when writing real code against it.) The return value is `MB_SUCCESS` or `MB_FAILURE`; the final `int *error` parameter receives a detailed numeric error code from the `MB_ERROR_*` family (defined in `mb_status.h`) whenever the return value is `MB_FAILURE`, and is set to `MB_ERROR_NO_ERROR` on success. `void **mbio_ptr` is a **pointer to a pointer** — the function allocates a new `mb_io_struct` internally and writes *its* address back into the caller's pointer variable, through this extra layer of indirection; this is the same mechanism from Section 9.6, applied to a pointer value rather than a `double`, and it is why the parameter type has two `*`s rather than one.

### 18.3 `mb_io_struct`: The Central Bookkeeping Structure

Every open swath file, throughout its lifetime in a running MB-System program, has exactly one associated `mb_io_struct` instance, allocated by `mb_read_init` and freed by the corresponding `mb_close`. Conceptually, and considerably simplified for teaching purposes, it holds:

- The open file's `FILE *` handle (or equivalent lower-level file descriptor).
- The numeric format code this file was opened as.
- Buffers holding the *current* ping's data — beam depths, positions, times, flags — refreshed each time `mb_read` is called.
- A `void *` pointer to a format-specific "store" structure (an `mbsys_*`-defined type), holding whatever raw, manufacturer-specific fields that particular format's reader needs to track beyond MBIO's own generic fields.

The `void *store_data` pointer is a particularly important pattern to understand: `void *` means "a pointer to memory of unspecified type," letting one single, generic `mb_io_struct` field hold a pointer to *any* of the dozens of different, format-specific structure types MB-System supports, without `mb_io_struct` itself needing to know, at compile time, which one it will actually be pointing at for any given open file. Each format-specific reader function, which *does* know its own structure type, casts this `void *` back to the correct specific pointer type before using it — a technique called **type punning through `void *`**, and it is the core mechanism by which MBIO achieves format independence at its generic API layer while still supporting arbitrarily different binary record layouts underneath.

### 18.4 Function-Pointer-Based Format Dispatch

Building on the `switch`-based teaching example from Part 7, real MBIO format dispatch is implemented through arrays of **function pointers** — variables that hold the address of a function itself, rather than the address of ordinary data:

```c
typedef int (*read_function_t)(int verbose, void *mbio_ptr, void *store_ptr, int *error);

read_function_t reader_for_format[300];   /* indexed by format ID; simplified illustration */

reader_for_format[88]  = mbr_rd_em710;
reader_for_format[261] = mbr_rd_kemkmall;
```

Once such a table is populated, calling the *correct* reader for whichever format a given file was opened as becomes a single, uniform call: `reader_for_format[format](verbose, mbio_ptr, store_ptr, &error);` — no `switch` or `if` chain is needed at the actual call site, and adding support for an entirely new format becomes a matter of writing the new `mbr_rd_newformat` function and adding one new table entry, without touching the generic dispatch logic at all. This is a genuinely more advanced C technique than anything covered so far in this primer, and Part 20 revisits function pointers in more general depth; it is introduced here specifically because recognizing this pattern is essential to understanding how `mb_format.c` and its neighbors actually route a generic `mb_read()` call down into one specific format's reader.

---

## Part 19 — Putting It Together: Reading a Real Multibeam File

With pointers, structures, dynamic memory, and file I/O all now available, we can write a small, genuinely functional program using the real MBIO API to open an actual multibeam file and report basic statistics about it — the first program in this primer that is not simplified or simulated.

```c
/* mbsummary.c
 *
 * Opens a swath file using MBIO, reads every ping, and reports
 * the number of pings, total beams, and average good-beam depth.
 *
 * Build (after installing MB-System via CMake, Part 2):
 *   gcc -Wall -Wextra -g -I/usr/local/mbsystem/include mbsummary.c \
 *       -o mbsummary -L/usr/local/mbsystem/lib -lmbio -lm
 */

#include <stdio.h>
#include <stdlib.h>

#include "mb_status.h"
#include "mb_define.h"
#include "mb_io.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <filename> <format>\n", argv[0]);
        return 1;
    }

    char *file = argv[1];
    int   format = atoi(argv[2]);   /* atoi: convert a command-line string to int */

    int    verbose = 1;
    int    error = MB_ERROR_NO_ERROR;
    void  *mbio_ptr = NULL;

    int    pings_default = 1;
    double bounds_default[4] = {-360.0, 360.0, -90.0, 90.0};
    double btime_d = -1.0e10, etime_d = 1.0e10;
    double speedmin = 0.0, timegap = 1000000.0;

    int beams_bath = 0, beams_amp = 0, pixels_ss = 0;

    int status = mb_read_init(verbose, file, format,
                               pings_default, bounds_default,
                               btime_d, etime_d, speedmin, timegap,
                               &mbio_ptr, &btime_d, &etime_d,
                               &beams_bath, &beams_amp, &pixels_ss, &error);

    if (status == MB_FAILURE) {
        fprintf(stderr, "Failed to open %s (format %d): error %d\n", file, format, error);
        return 1;
    }

    /* Allocate arrays sized to this file's actual beam/pixel counts. */
    double *bath = (double *)malloc(beams_bath * sizeof(double));
    double *bathacrosstrack = (double *)malloc(beams_bath * sizeof(double));
    double *bathalongtrack  = (double *)malloc(beams_bath * sizeof(double));
    char   *beamflag        = (char *)malloc(beams_bath * sizeof(char));
    double *amp              = (double *)malloc(beams_amp * sizeof(double));
    double *ss                = (double *)malloc(pixels_ss * sizeof(double));
    double *ssacrosstrack     = (double *)malloc(pixels_ss * sizeof(double));
    double *ssalongtrack      = (double *)malloc(pixels_ss * sizeof(double));

    if (!bath || !bathacrosstrack || !bathalongtrack || !beamflag) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    int    npings = 0;
    long   total_good_beams = 0;
    double sum_depth = 0.0;

    int    kind, nbath, namp, nss;
    double time_d, navlon, navlat, speed, heading;
    double distance, altitude, sensordepth;
    double roll, pitch, heave;

    do {
        status = mb_get(verbose, mbio_ptr, &kind, &pings_default,
                         &time_d, &navlon, &navlat, &speed, &heading,
                         &distance, &altitude, &sensordepth,
                         &roll, &pitch, &heave,
                         &beams_bath, &beams_amp, &pixels_ss,
                         beamflag, bath, amp,
                         bathacrosstrack, bathalongtrack,
                         ss, ssacrosstrack, ssalongtrack,
                         NULL, &error);

        if (status == MB_SUCCESS && kind == MB_DATA_DATA) {
            npings++;
            for (int i = 0; i < beams_bath; i++) {
                if (beamflag[i] == MB_FLAG_NONE) {   /* MB_FLAG_NONE: an unflagged, good beam */
                    sum_depth += bath[i];
                    total_good_beams++;
                }
            }
        }

    } while (error <= MB_ERROR_NO_ERROR);   /* MBIO error convention: values <= 0 mean "keep going" */

    printf("File: %s\n", file);
    printf("Pings read: %d\n", npings);
    printf("Total good beams: %ld\n", total_good_beams);
    if (total_good_beams > 0) {
        printf("Average good-beam depth: %.3f m\n", sum_depth / total_good_beams);
    }

    free(bath);
    free(bathacrosstrack);
    free(bathalongtrack);
    free(beamflag);
    free(amp);
    free(ss);
    free(ssacrosstrack);
    free(ssalongtrack);

    mb_close(verbose, &mbio_ptr, &error);

    return 0;
}
```

Several important notes belong alongside this listing. **First**, the exact parameter list of `mb_read_init` and `mb_get` shown here is simplified and illustrative of the real API's *shape and convention*, written from general knowledge of MBIO's structure rather than copied from current source text; the real, exact, currently correct signatures are declared in `mb_io.h` in your own MB-System checkout, and you should always compile against and consult that header directly rather than treating this listing as byte-exact truth — treat this program as a guided template for the pattern, to be corrected against the real headers on your machine before it will actually compile and run. **Second**, notice every allocation is sized using `beams_bath`, `beams_amp`, and `pixels_ss` — values that `mb_read_init` itself determined by inspecting the actual file, exactly the dynamic-sizing technique from Part 14, replacing any fixed, guessed maximum. **Third**, the main loop's `do { ... } while (error <= MB_ERROR_NO_ERROR);` condition is a real instance of the read-check-process-loop pattern previewed back in Part 8, now driven by MBIO's actual error convention rather than a simulated flag: `MB_ERROR_NO_ERROR` is conventionally zero, "less than zero" values (by MB-System's convention) represent informational/non-fatal conditions worth continuing past, and reaching end-of-file or a genuine fatal error produces a positive error code that ends the loop. **Fourth**, every `malloc`'d array is paired with exactly one corresponding `free` call near the end, and `mb_close` releases the `mb_io_struct` itself — the full allocate/use/release lifecycle from Part 14, applied to a real, non-trivial program for the first time in this primer.

---

## Part 20 — Intermediate Topics

### 20.1 Bitwise Operators

Distinct from the logical operators in Part 5, **bitwise** operators manipulate the individual bits of an integer value directly — essential when parsing binary sonar records that pack several small flag or status fields into a single byte or word to save space:

| Operator | Meaning |
|---|---|
| `&` | bitwise AND |
| \| | bitwise OR |
| `^` | bitwise XOR |
| `~` | bitwise NOT (complement) |
| `<<` | shift left |
| `>>` | shift right |

```c
uint8_t status_byte = 0b00001010;   /* imagine this came from a raw record header */

int bit1_set = (status_byte & 0x02) != 0;   /* test bit 1 (value 2) */
int bit3_set = (status_byte & 0x08) != 0;   /* test bit 3 (value 8) */

uint8_t with_bit0_set = status_byte | 0x01;   /* set bit 0 without disturbing other bits */
uint8_t with_bit1_cleared = status_byte & ~0x02;   /* clear bit 1 without disturbing others */
```

This exact style of masking (`&`), setting (`|`), and clearing (`& ~`) individual bits is how real format-reading code extracts, for instance, several independent Boolean flags that a sonar manufacturer has packed together into one status word, rather than giving each flag its own full byte.

### 20.2 `enum`: Named Integer Constants

An `enum` gives a set of related integer constants readable names, avoiding "magic numbers" scattered through code:

```c
enum BeamQuality {
    BEAM_GOOD = 0,
    BEAM_FLAGGED_MANUAL = 1,
    BEAM_FLAGGED_FILTER = 2,
    BEAM_FLAGGED_SONAR = 3
};

enum BeamQuality quality = BEAM_FLAGGED_FILTER;

if (quality == BEAM_GOOD) {
    /* ... */
}
```

Unless you specify otherwise, an `enum`'s members are automatically numbered 0, 1, 2, ... in declaration order; MB-System's own `MB_ERROR_*` and `MB_DATA_*` families of constants are conceptually exactly this kind of named-integer scheme (whether implemented via `enum` or via `#define`, depending on the specific header and its era).

### 20.3 `union`: Overlapping Storage

A `union` looks syntactically like a `struct`, but all its members share the *same* memory location rather than each having its own — a `union`'s total size is only as large as its single largest member, and writing to one member overwrites whatever was stored via any other member. Unions appear in some binary-format-parsing code as a way to interpret the same raw bytes as different types depending on context (for instance, reading 4 raw bytes once as a `float` and, in a different code path, as a `uint32_t` for byte-swapping purposes) — a genuinely advanced, somewhat dangerous technique that requires exact understanding of both members' byte layouts, and one you are more likely to *read* in mature binary-format code than to need to write yourself while learning.

### 20.4 `static`

`static`, applied to a variable *inside* a function, makes that variable retain its value between separate calls to the function (instead of being freshly created and destroyed every time, as ordinary local variables are) — useful for a running counter or a one-time initialization flag. `static`, applied to a function or global variable *at file scope* (outside any function), restricts that function or variable's visibility to only the current `.c` file, preventing other files in the same program from accidentally calling it or reading it directly — the closest C comes to a "private" or "internal" designation, and a convention used extensively throughout MB-System's `.c` files for helper functions that are implementation details of one file, never meant to be part of that file's externally usable API.

### 20.5 Function Pointers, in Full

Section 18.4 introduced function pointers in the specific context of format dispatch; here is the general syntax, standalone:

```c
int add(int a, int b) { return a + b; }
int subtract(int a, int b) { return a - b; }

int (*operation)(int, int);   /* declares a pointer to a function taking two ints, returning int */

operation = add;
printf("%d\n", operation(3, 4));   /* prints 7 */

operation = subtract;
printf("%d\n", operation(3, 4));   /* prints -1 */
```

The type `int (*operation)(int, int)` reads, from the inside out: "`operation` is a pointer to a function that takes two `int`s and returns `int`." Once assigned, calling `operation(3, 4)` calls whichever function `operation` currently points at — exactly the mechanism that lets a table of function pointers, indexed by format ID, replace a large `switch` statement with a single, uniform call site, as shown in Part 18.

### 20.6 `const` Correctness With Pointers

`const` interacts with pointers in two genuinely distinct ways, worth distinguishing carefully:

```c
const double *ptr_to_const;   /* the VALUE pointed at cannot be modified through ptr_to_const */
double *const const_ptr;      /* the POINTER ITSELF cannot be reassigned to point elsewhere */
const double *const both;     /* neither the value nor the pointer can be changed */
```

You will see `const double *` extensively in MBIO function signatures for parameters that a function is only meant to *read* and never modify — an important, compiler-enforced signal to any caller (and any future reader of the code) about that parameter's intended, one-directional data flow.

---

## Part 21 — Debugging and Tools

### 21.1 Compiler Warnings Are Your First Debugger

Revisit Part 17's `-Wall -Wextra` flags: a large fraction of real C bugs — uninitialized variables, mismatched `printf` format specifiers, comparisons between signed and unsigned types with surprising results, unused variables that suggest a forgotten line of logic — are caught by the compiler itself, for free, before your program ever runs, provided you have these warning flags enabled. Never ignore a warning without first understanding exactly why the compiler raised it.

### 21.2 Using a Debugger (`lldb` on macOS, `gdb` on Linux)

```sh
gcc -g -O0 mbsummary.c -o mbsummary -I... -L... -lmbio -lm
lldb ./mbsummary
(lldb) run file1.all 88
(lldb) breakpoint set --name mb_get
(lldb) print beams_bath
```

`-g` embeds debugging symbols; `-O0` disables compiler optimizations that would otherwise reorder or eliminate code in ways that make step-by-step debugging confusing. A debugger lets you pause execution at a specific line (a **breakpoint**), inspect the current value of any variable, and step through your program one line at a time — invaluable for understanding exactly what a real MBIO call sequence is doing internally, and for tracking down the exact point where a value diverges from what you expected.

### 21.3 Detecting Memory Errors: `valgrind` / AddressSanitizer

Because C provides no automatic protection against the memory bugs covered in Parts 10, 11, and 14 (out-of-bounds access, use-after-free, memory leaks), specialized tools exist specifically to detect them at runtime. On Linux, `valgrind ./mbsummary file1.all 88` runs your program inside an instrumented environment that reports memory errors and leaks in detail. On macOS (where classic `valgrind` support is limited on recent versions), the equivalent, built directly into Clang, is **AddressSanitizer**:

```sh
gcc -g -fsanitize=address mbsummary.c -o mbsummary -I... -L... -lmbio -lm
./mbsummary file1.all 88
```

If the program contains a buffer overflow, use-after-free, or similar memory error, AddressSanitizer prints a detailed report identifying the exact line and the nature of the violation, immediately when it occurs — dramatically faster to diagnose than the "program crashed somewhere, sometime later, for reasons unclear" failure mode these bugs otherwise produce.

### 21.4 A Practical Debugging Workflow

When something in an MBIO-based program behaves unexpectedly: first, recompile with every warning flag enabled and address every warning; second, recompile with AddressSanitizer enabled and run against the smallest input file that still reproduces the problem; third, if the problem persists, use a debugger to set a breakpoint at the earliest point you suspect and step forward, watching specific variables, until behavior diverges from what you expect. This progression — warnings, then sanitizers, then interactive debugging — resolves the overwhelming majority of real bugs you will encounter, roughly in order of effort required, and mirrors the evidence-driven, hands-on troubleshooting approach you already bring to your existing marine-data software work.

---

## Part 22 — Where to Go Next

You now have a complete, working foundation across the entire core C language: compilation and the CMake build system, all of C's basic and derived types, every major control-flow construct, functions and the error-handling convention that pervades MBIO, arrays, pointers and their deep relationship to arrays, strings and their attendant buffer-safety concerns, structures (including the conceptual shape of `mb_io_struct` itself), dynamic memory allocation and its associated bugs, binary file I/O including endianness, the preprocessor, and a set of genuinely intermediate topics — bitwise operators, enums, unions, `static`, function pointers, and `const` correctness — that appear throughout real, mature MBIO and utility source code.

From here, productive next steps include: reading `src/mbio/mb_format.c` and `src/mbio/mb_read.c` directly, using the guided "skim signatures first, then read one function closely" strategy this primer has modeled throughout; picking one specific, real format reader (for example, whichever one corresponds to a sonar system you already work with, such as a Kongsberg EM-family or KMALL reader) and reading it start to finish now that its structures, error handling, and dynamic allocation patterns are no longer unfamiliar syntax; and, once comfortable, attempting a small, real, well-scoped modification — fixing a minor documented bug, adding a small diagnostic print, or extending one tool's command-line options — and validating it by rebuilding MB-System via CMake and testing against real data, exactly the evidence-driven, iterative workflow you already apply in your existing software work.
