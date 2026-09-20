# Perl Primer for MB-System Programming

*A beginner-to-intermediate introduction to Perl, taught through MB-System's `mbm_*` macro scripts.*

---

## How to Use This Primer

This primer assumes no prior Perl experience, though it assumes the general programming comfort you already have from C, Python, and marine-data tooling work. Every concept is explained from first principles, and every non-trivial example is built around MB-System's real macro conventions rather than generic Perl exercises.

MB-System is a collaborative effort between the Monterey Bay Aquarium Research Institute (MBARI), the Center for Marine Environmental Sciences (MARUM) at Universität Bremen, and the Center for Coastal and Ocean Mapping (CCOM) at the University of New Hampshire. Alongside its C-based MBIO library (covered in the companion *C Primer for MB-System Programming*), MB-System ships a large family of **macros** — scripts named `mbm_*`, living in `src/macros/` — that are primarily written in **Perl**, with some newer additions written in Python [web:31]. These macros do not process sonar bytes directly the way MBIO's C code does. Instead, each one **generates an executable shell script** that itself calls a chain of MB-System command-line tools and GMT (Generic Mapping Tools) commands, producing grids, plots, GeoTIFFs, and other finished outputs [web:27][web:31]. Well-known examples include `mbm_grid` (generates a shellscript that builds a bathymetry or backscatter grid/mosaic), `mbm_plot` (generates a shellscript that builds a GMT swath plot), `mbm_grdplot`, `mbm_grd3dplot`, `mbm_xyplot`, and `mbm_histplot` [web:31][web:34].

Perl is a natural fit for this "generate-a-script" role: it is exceptionally strong at text processing, string manipulation, and file generation — exactly what is needed to assemble a syntactically correct, parameterized shell script from a template, populated with a user's chosen options. Learning Perl through these macros means learning the language in the context it was actually chosen for, rather than in the abstract.

---

## Table of Contents

- Part 1 — Why Perl, and What MB-System Macros Actually Do
- Part 2 — Setting Up and Running Perl
- Part 3 — Scalars: Perl's Basic Variables
- Part 4 — Arrays
- Part 5 — Hashes
- Part 6 — Control Flow
- Part 7 — Subroutines
- Part 8 — Regular Expressions and Text Processing
- Part 9 — File I/O and Generating Shell Scripts
- Part 10 — Command-Line Argument Parsing
- Part 11 — Anatomy of a Real `mbm_*` Macro
- Part 12 — Writing Your Own MB-System Macro
- Part 13 — Debugging and Common Pitfalls
- Part 14 — Where to Go Next

---

## Part 1 — Why Perl, and What MB-System Macros Actually Do

### 1.1 The Macro Layer, Conceptually

MB-System's command-line tools (`mbinfo`, `mbgrid`, `mblist`, `mbclean`, and dozens more, mostly written in C) each do one focused job well, but a real survey workflow — say, "grid this set of swath files, then plot the grid with shaded relief and contours, then export a GeoTIFF" — requires calling several of these tools in sequence, with carefully matched parameters (grid bounds must match between the gridding step and the plotting step, projection choices must agree, and so on). Rather than asking every user to hand-write this multi-step shell pipeline correctly from scratch every time, MB-System provides **macros**: Perl scripts that accept a smaller set of high-level options, work out the correct detailed parameters, and **write out a complete, ready-to-run shell script** that performs the whole pipeline. That generated shell script is typically left on disk (for example, `mbm_grid` produces a script you then run separately), so you can inspect exactly what commands it will run, and rerun it without invoking the macro again [web:32][web:27].

### 1.2 Why Perl Specifically

Perl was designed from the outset for exactly the kind of task these macros perform: reading text, matching patterns within it, substituting values into templates, and writing out new text — all with a terse, powerful, built-in syntax that avoids the ceremony of calling external string-processing libraries. Generating a syntactically valid shell script that correctly quotes filenames, substitutes numeric bounds, and conditionally includes or omits blocks of commands based on user-supplied flags is a natural fit for Perl's string interpolation, regular expressions, and list-processing features, which is why MB-System's original macro authors reached for Perl rather than C for this particular layer of the system, and why current development continues to maintain and extend those Perl macros (with some newer ones written in Python) [web:31].

### 1.3 What You Will Be Able to Do After This Primer

By the end, you will be able to read any `mbm_*.perl` source file in `src/macros/` and understand exactly how it parses its command-line options, computes derived values (grid bounds, GMT projection strings, output filenames), and assembles the final shell script it writes out — and you will be able to write a small, original macro of your own that follows the same conventions.

---

## Part 2 — Setting Up and Running Perl

### 2.1 Checking for Perl

Perl ships pre-installed on macOS and virtually every Linux distribution. Check your version:

```sh
perl -v
```

MB-System's macros target modern Perl 5 (Perl 5.x, the version line in universal use today — "Perl 6" was later renamed Raku and is a distinct language, unrelated to anything used in MB-System).

### 2.2 Running a Perl Script

A Perl script is a plain text file, conventionally given no required extension for an executable command-line tool (MB-System's own installed `mbm_grid` command has no `.pl` suffix) but often `.perl` or `.pl` during development. Two ways to run one:

```sh
perl myscript.pl arg1 arg2
```

or, if the file begins with a **shebang line** and has execute permission:

```perl
#!/usr/bin/env perl
```

```sh
chmod +x myscript.pl
./myscript.pl arg1 arg2
```

The shebang line tells the operating system's shell which interpreter to run the file with when it is executed directly, without a leading `perl` command — exactly how installed `mbm_*` commands are run in practice, as ordinary executables.

### 2.3 A First Perl Program

```perl
#!/usr/bin/env perl
use strict;
use warnings;

print "Hello from an MB-System-style Perl macro\n";
```

`use strict;` and `use warnings;` are not optional decoration — they should appear at the top of essentially every Perl script you write. `use strict` forces you to declare every variable explicitly before using it (catching typos that would otherwise silently create a new, unintended variable), and `use warnings` enables detailed diagnostic messages for dozens of common mistakes (using an uninitialized value, comparing values in a way that is probably not what you meant, and so on). Real MB-System macros consistently begin this way; treat these two lines as mandatory.

---

## Part 3 — Scalars: Perl's Basic Variables

### 3.1 What a Scalar Is

A **scalar** holds a single value — a number, a string, or a reference (Part 4 introduces references) — and is always written with a leading `$` sigil:

```perl
my $survey_name = "Bremen_Bay_2026";
my $grid_cell_size = 5.0;
my $nfiles = 12;
```

`my` declares a new variable, scoped to the enclosing block (or the whole file, if at the top level) — required under `use strict`. Unlike C, Perl scalars have no separate "int vs. float vs. string" declaration: the *same* `$x` can hold a number now and a string later; Perl converts between numeric and string representations automatically, based on how a value is used in a given expression.

### 3.2 Numbers

```perl
my $cell_size_m = 5;
my $bounds_west = -8.90;
my $result = $cell_size_m * 2 + 1;   # ordinary arithmetic: + - * / ** %
```

### 3.3 Strings

```perl
my $format_flag = "-F-1";
my $filename    = 'raw_data.mb88';    # single quotes: no interpolation
my $message     = "Processing $filename with flag $format_flag";  # double quotes: interpolation
```

**Double-quoted** strings interpolate variables directly inside them — `"$filename"` is replaced with the variable's current value. **Single-quoted** strings do not interpolate anything; `'$filename'` remains the literal four characters `$`, `f`, `i`, `l`, etc. This distinction matters constantly when generating shell script text: you must be deliberate about which parts of your output should have Perl variables substituted in, and which parts (like a literal `$1` meant for the *generated shell script* to interpret later, not for Perl itself) must be protected from Perl's own interpolation using single quotes or an escaped `\$`.

```perl
my $ps_file = "survey_grid.ps";
print "The plot will call: mbm_plot -F1 -O $ps_file\n";
```

### 3.4 String Concatenation and Repetition

```perl
my $base = "mbm_grid";
my $cmd  = $base . " -I datalist.mb-1 -O grid_output";   # . concatenates strings
```

### 3.5 Undefined Values and Defaults

A scalar that has been declared but never assigned holds the special value `undef`. Checking for it, and supplying a default, is extremely common when parsing optional command-line options:

```perl
my $cell_size = undef;
$cell_size = 5.0 unless defined($cell_size);   # supply a default if none was given
```

The `//=` operator (the "defined-or" assignment) does the same thing more compactly:

```perl
$cell_size //= 5.0;
```

---

## Part 4 — Arrays

### 4.1 What an Array Is

An **array** holds an ordered list of scalars, written with a leading `@` sigil when referring to the whole array, and accessed element-by-element with `$` (because each individual element is itself a scalar):

```perl
my @input_files = ("EM710_0001.mb88", "EM710_0002.mb88", "EM710_0003.mb88");

print $input_files[0];   # first element -- note the $ sigil here, not @
print scalar(@input_files);   # the array's length, forced into scalar context: 3
```

Array indices start at 0, exactly as in C. `$#input_files` gives the index of the *last* element (length minus one), a common source of off-by-one confusion for newcomers.

### 4.2 Building Arrays Incrementally

```perl
my @good_files = ();

foreach my $file (@input_files) {
    if (-e $file) {          # -e is a "file test operator": true if the file exists
        push(@good_files, $file);
    }
}
```

`push` appends one or more elements onto the end of an array — the most common way MB-System macros accumulate a growing list of validated input filenames, generated command lines, or warning messages as a script processes its arguments.

### 4.3 Common Array Operations

| Operation | Effect |
|---|---|
| `push(@a, $x)` | append `$x` to the end |
| `pop(@a)` | remove and return the last element |
| `shift(@a)` | remove and return the first element |
| `unshift(@a, $x)` | prepend `$x` to the beginning |
| `sort(@a)` | returns a new, sorted copy |
| `reverse(@a)` | returns a new, reverse-order copy |
| `join($sep, @a)` | joins all elements into one string, separated by `$sep` |
| `split($sep, $s)` | splits a string into an array, on a separator |

`join` and `split` are used constantly when generating and parsing command-line strings:

```perl
my @options = ("-F-1", "-I", "datalist.mb-1", "-O", "grid_out");
my $full_command = join(" ", @options);   # "-F-1 -I datalist.mb-1 -O grid_out"
```

### 4.4 `@ARGV`: The Command-Line Arguments

Just as C's `argv` array (Part 6 of the C primer) holds a program's command-line arguments, Perl automatically populates a special built-in array named `@ARGV` with exactly that:

```perl
#!/usr/bin/env perl
use strict; use warnings;

print "Number of arguments: " . scalar(@ARGV) . "\n";
foreach my $arg (@ARGV) {
    print "  got: $arg\n";
}
```

Note that, unlike C's `argv[0]`, Perl's `@ARGV` does **not** include the script's own name — `@ARGV`'s first element (`$ARGV[0]`) is the *first actual argument* the user typed. The script's own name, if needed, is available separately as the special variable `$0`.

### 4.5 `foreach`, in Depth

```perl
foreach my $file (@input_files) {
    print "Processing $file\n";
}
```

`$file` here is a fresh, block-scoped alias for each element of `@input_files` in turn — critically, it is an **alias**, not a copy: modifying `$file` inside the loop actually modifies the corresponding element of `@input_files` itself, a Perl-specific behavior worth being deliberate about.

---

## Part 5 — Hashes

### 5.1 What a Hash Is

A **hash** is an unordered collection of key-value pairs, written with a leading `%` sigil for the whole hash, and accessed by key with `$` and curly braces:

```perl
my %options = (
    "cellsize"   => 5.0,
    "boundaries" => "-8.9/-8.5/53.0/53.4",
    "outfile"    => "grid_out",
);

print $options{"cellsize"};   # 5.0
$options{"projection"} = "u32N/1:50000";   # add a new key
```

Hashes are exactly how MB-System macros commonly store the full set of parsed command-line options internally, before using those values to build the generated shell script's text — far more readable than tracking a dozen separate scalar variables individually.

### 5.2 Checking Whether a Key Exists

```perl
if (exists($options{"projection"})) {
    print "A projection was specified: $options{'projection'}\n";
} else {
    print "No projection specified; a default will be used.\n";
}
```

`exists` checks whether a key is present at all, which is subtly different from checking `defined($options{"projection"})` — a key can exist in a hash with an explicitly stored `undef` value, in which case `exists` is true but `defined` is false. This distinction rarely matters for simple option-parsing but is worth knowing before it surprises you.

### 5.3 Iterating Over a Hash

```perl
foreach my $key (sort keys %options) {
    print "$key => $options{$key}\n";
}
```

`keys %options` returns an array of all the hash's keys; `sort`ing them gives predictable, repeatable output order, which matters when a macro is printing a diagnostic summary of the options it parsed, since hashes themselves have no guaranteed internal ordering.

---

## Part 6 — Control Flow

### 6.1 `if` / `elsif` / `else`

```perl
if ($cell_size <= 0) {
    die "Error: cell size must be positive\n";
} elsif ($cell_size < 1.0) {
    print "Warning: very fine cell size, this may take a long time\n";
} else {
    print "Cell size looks reasonable\n";
}
```

`die` immediately terminates the script, printing its message to standard error and exiting with a nonzero status — Perl's rough equivalent of a fatal `return 1;` combined with an error message, and the standard way MB-System macros abort when given invalid or missing required arguments.

### 6.2 Truth in Perl

Perl treats the following as **false**: the number `0`, the string `"0"`, the empty string `""`, and `undef`. Every other value — including the string `"0.0"` (a nonempty string, even though it numerically represents zero) — is **true**. This is a common early trap: a string `"0.0"` read from a config file is truthy in a plain `if ($value)` test, unlike the pure integer `0`.

### 6.3 `unless`

Perl provides `unless` as a readable inverse of `if`, used somewhat idiomatically:

```perl
die "Error: input file not found\n" unless (-e $input_file);
```

### 6.4 Loops: `while`, `until`, `for`

```perl
my $i = 0;
while ($i < scalar(@input_files)) {
    print "File $i: $input_files[$i]\n";
    $i++;
}

for (my $i = 0; $i < scalar(@input_files); $i++) {
    print "File $i: $input_files[$i]\n";
}
```

The C-style `for` loop's three-part header works identically to C's. `foreach` (Part 4) is generally preferred over a manually indexed `for`/`while` when simply visiting every array element, since it is shorter and eliminates any possibility of an off-by-one indexing mistake.

---

## Part 7 — Subroutines

### 7.1 Declaring and Calling

```perl
sub compute_grid_dimensions {
    my ($west, $east, $south, $north, $cellsize) = @_;

    my $ncols = int(($east - $west) / $cellsize) + 1;
    my $nrows = int(($north - $south) / $cellsize) + 1;

    return ($ncols, $nrows);
}

my ($ncols, $nrows) = compute_grid_dimensions(-8.90, -8.50, 53.00, 53.40, 0.005);
print "Grid will be $ncols columns by $nrows rows\n";
```

Every subroutine's arguments arrive, without exception, inside the special built-in array `@_`. `my ($west, $east, $south, $north, $cellsize) = @_;` is the standard first line of a Perl subroutine, unpacking `@_`'s elements, in order, into individually named local variables — Perl has no separate named-parameter-list syntax the way C does; this unpacking idiom is how MB-System's Perl macros achieve the same readability.

### 7.2 Returning Multiple Values

Section 7.1 already showed it: `return ($ncols, $nrows);` returns a list, which the caller can capture into multiple variables at once, `my ($ncols, $nrows) = compute_grid_dimensions(...)`. This is considerably more direct than C's pointer-parameter workaround (Part 9 of the C primer) for the same underlying need, and is one of the areas where Perl's design noticeably reduces boilerplate compared to C for exactly the kind of small utility calculations MB-System macros perform constantly (computing bounds, dimensions, filenames).

### 7.3 Default and Optional Arguments

```perl
sub build_plot_command {
    my ($input_file, $output_file, $projection) = @_;
    $projection //= "M6i";   # default Mercator projection, 6 inches wide, if none given

    return "mbm_plot -I $input_file -O $output_file -Jm$projection";
}
```

### 7.4 Subroutines That Modify the Caller's Data (References, Briefly)

Perl passes arguments to subroutines by reference for arrays and hashes when they are passed as a whole (not element-by-element), meaning a subroutine can, if written carefully, modify the caller's original array or hash directly — conceptually similar to C's pointer-based "modify the caller's variable" pattern from the C primer's Part 9, though Perl's exact mechanics differ and are covered more fully in intermediate Perl references material beyond this primer's scope. For this primer's purposes, the safe, clear default is: **have subroutines return new values via `return`, rather than relying on argument mutation**, exactly as every example above does.

---

## Part 8 — Regular Expressions and Text Processing

This is Perl's signature strength, and it is used throughout `mbm_*` macros for validating and parsing user input.

### 8.1 Matching a Pattern

```perl
my $filename = "EM710_survey_0042.mb88";

if ($filename =~ /\.mb(\d+)$/) {
    my $format_code = $1;
    print "Detected MB-System format code: $format_code\n";
}
```

`=~` applies a **regular expression** (a pattern describing text to search for) to a string. `/\.mb(\d+)$/` reads as: a literal `.`, then the literal letters `mb`, then one or more digits (`\d+`), captured by the parentheses, then the end of the string (`$`). If the match succeeds, `$1` automatically holds whatever text the first set of parentheses captured — here, the format code digits.

### 8.2 Common Regex Building Blocks

| Pattern | Matches |
|---|---|
| `.` | any single character |
| `\d` | any digit |
| `\s` | any whitespace character |
| `\w` | any "word" character (letter, digit, underscore) |
| `+` | one or more of the preceding element |
| `*` | zero or more of the preceding element |
| `?` | zero or one of the preceding element |
| `^` | start of the string |
| `$` | end of the string |
| `( )` | groups a subpattern, and captures it into `$1`, `$2`, ... |
| `[abc]` | any one of the listed characters |

### 8.3 Substitution

```perl
my $command_template = "mbm_grid -I INPUTFILE -O OUTPUTFILE -A CELLSIZE";

$command_template =~ s/INPUTFILE/datalist.mb-1/;
$command_template =~ s/OUTPUTFILE/survey_grid/;
$command_template =~ s/CELLSIZE/5.0/;

print "$command_template\n";
```

`s/pattern/replacement/` substitutes the first match of `pattern` with `replacement` in place. This exact template-and-substitute technique — starting from a fixed skeleton string containing placeholder tokens, then substituting in user-supplied values — is a simple, readable alternative to heavy string interpolation for generating longer, more complex shell-script command lines, and appears in various forms throughout the `mbm_*` macro sources.

### 8.4 Global Substitution and Splitting on Patterns

```perl
my $bounds_string = "  -8.90 / -8.50 / 53.00 / 53.40  ";
$bounds_string =~ s/^\s+|\s+$//g;      # trim leading/trailing whitespace, globally
my @bounds = split(/\s*\/\s*/, $bounds_string);   # split on "/" with optional surrounding spaces

my ($west, $east, $south, $north) = @bounds;
print "West: $west, East: $east, South: $south, North: $north\n";
```

The `g` flag on a substitution makes it apply to *every* match in the string, not just the first — essential here, since there are two separate whitespace regions to trim. `split` with a regex separator, rather than a fixed literal string, is exactly how a macro parses a user-supplied bounds string (like `mbm_grid`'s `-R` region option) into its four separate numeric components, tolerating whatever spacing the user happened to type around the slashes.

### 8.5 Validating Numeric Input

```perl
sub is_valid_number {
    my ($value) = @_;
    return ($value =~ /^-?\d+(\.\d+)?$/);
}

die "Error: cell size must be numeric\n" unless is_valid_number($cell_size);
```

This pattern — `^-?\d+(\.\d+)?$` — matches an optional leading minus sign, one or more digits, and an optional decimal point followed by more digits, anchored to the whole string (`^...$`) so that no extra, invalid trailing characters sneak past. Validating every user-supplied numeric option this way, before it is ever substituted into a generated shell script, is exactly the kind of defensive check real macros perform, since an unvalidated, malformed value substituted directly into shell-script text could otherwise produce a broken or even dangerous generated script.

---

## Part 9 — File I/O and Generating Shell Scripts

### 9.1 Opening a File for Writing

```perl
open(my $fh, ">", "grid_script.cmd") or die "Cannot open grid_script.cmd: $!\n";

print $fh "#!/bin/sh\n";
print $fh "# Auto-generated by mbm_grid-style macro\n";
print $fh "mbgrid -I datalist.mb-1 -A2 -E5.0/5.0 -O survey_grid\n";

close($fh);
```

`open(my $fh, ">", "filename")` opens `"filename"` for writing (`">"` truncates/creates; `">>"` would append instead), storing a **filehandle** in `$fh`. `or die "...$!\n"` is the standard Perl idiom for handling a failed operation: if `open` fails, the `or die` half of the expression runs, printing an error message that includes `$!` — a special built-in variable automatically holding the operating system's own description of the most recent error (e.g., "Permission denied" or "No such file or directory") — and terminates the script. You will see `or die "...$!"` after nearly every file operation in careful Perl code, MB-System macros included.

### 9.2 Making the Generated Script Executable

Real `mbm_*` macros make their generated shell scripts directly runnable, exactly as `mbm_grid` and `mbm_plot` do for the user [web:32][web:33]:

```perl
close($fh);
chmod(0755, "grid_script.cmd") or die "Cannot chmod grid_script.cmd: $!\n";
print "Wrote executable script: grid_script.cmd\n";
```

`chmod(0755, ...)` sets Unix permissions (owner: read/write/execute; group and others: read/execute) — the octal `0755` notation mirrors exactly what you would type at a shell prompt with `chmod 755 filename`.

### 9.3 Heredocs: Writing Large Blocks of Script Text Cleanly

For a shell script with many lines, calling `print $fh "...";` repeatedly for every single line becomes unwieldy. Perl's **heredoc** syntax lets you write a large, multi-line block of text as a single literal, with normal double-quote-style interpolation still applying:

```perl
my $input_datalist = "datalist.mb-1";
my $cell_size      = 5.0;
my $output_root    = "survey_grid";

my $script_text = <<"END_SCRIPT";
#!/bin/sh
# Auto-generated grid-building script
mbgrid -I $input_datalist -A2 -E${cell_size}/${cell_size} -O $output_root
mbm_grdplot -I ${output_root}.grd -G1 -C1
END_SCRIPT

open(my $fh, ">", "grid_script.cmd") or die "Cannot open grid_script.cmd: $!\n";
print $fh $script_text;
close($fh);
chmod(0755, "grid_script.cmd");
```

`<<"END_SCRIPT"` begins a heredoc: everything from the next line up to a line containing exactly `END_SCRIPT` is treated as one big string literal, with the variables inside it (`$input_datalist`, `$cell_size`, `$output_root`) interpolated exactly as they would be in an ordinary double-quoted string. `${cell_size}` (curly braces around the variable name) is used instead of plain `$cell_size` specifically because it is immediately followed by a literal `/` — the braces make the variable name's boundary unambiguous to Perl's parser, avoiding any risk of the interpolation reading further characters as part of the variable's name. This heredoc technique — building the entire generated shell script as one interpolated block — is, in essence, exactly what real `mbm_grid` and `mbm_plot` do internally, just at a larger and more parameterized scale, with many conditional blocks of script text included or omitted depending on which options the user supplied.

### 9.4 Conditionally Including Script Blocks

```perl
my $script_text = "#!/bin/sh\n";
$script_text .= "mbgrid -I $input_datalist -A2 -E${cell_size}/${cell_size} -O $output_root\n";

if ($make_plot) {
    $script_text .= "mbm_grdplot -I ${output_root}.grd -G1 -C1\n";
}

if ($make_tiff) {
    $script_text .= "mbm_grdtiff -I ${output_root}.grd -O ${output_root}\n";
}
```

`.=` is the string-concatenation-assignment operator — `$x .= $y;` means `$x = $x . $y;`. Building the final script text incrementally, with `if` blocks appending optional sections, is exactly how a real macro assembles a shell script whose exact contents depend on which command-line flags the user passed — for instance, `mbm_grid`'s handling of its optional plotting-related flags [web:32].

---

## Part 10 — Command-Line Argument Parsing

### 10.1 Manual Parsing With `@ARGV`

For a small number of options, looping over `@ARGV` directly, as MB-System's older macros often do, is straightforward:

```perl
#!/usr/bin/env perl
use strict; use warnings;

my $input_file;
my $cell_size = 5.0;
my $verbose = 0;

while (@ARGV) {
    my $arg = shift(@ARGV);

    if ($arg eq "-I") {
        $input_file = shift(@ARGV);
    } elsif ($arg eq "-A") {
        $cell_size = shift(@ARGV);
    } elsif ($arg eq "-V") {
        $verbose = 1;
    } else {
        die "Unrecognized argument: $arg\n";
    }
}

die "Error: -I <input file> is required\n" unless defined($input_file);

print "Input file: $input_file\n";
print "Cell size: $cell_size\n";
print "Verbose: $verbose\n";
```

`shift(@ARGV)` removes and returns the *first* remaining element each time, which is exactly why this `while (@ARGV)` loop works: each pass through the loop consumes one or two elements from the front of `@ARGV`, until none remain. `eq` is the **string** equality operator (as opposed to `==`, which compares numerically) — a distinction that does not exist in C, where `==` always compares numerically and strings require `strcmp`; forgetting to use `eq` for strings in Perl (accidentally writing `$arg == "-I"`) is a common early mistake, since it often does not produce an obvious error, just silently wrong comparisons.

### 10.2 `Getopt::Long`: A More Robust Alternative

For anything beyond a handful of simple flags, Perl's standard `Getopt::Long` module handles long-form options, values, and validation with far less hand-written parsing code:

```perl
#!/usr/bin/env perl
use strict; use warnings;
use Getopt::Long;

my $input_file;
my $cell_size = 5.0;
my $verbose = 0;

GetOptions(
    "input=s"    => \$input_file,
    "cellsize=f" => \$cell_size,
    "verbose"    => \$verbose,
) or die "Error parsing command-line options\n";

die "Error: --input <file> is required\n" unless defined($input_file);

print "Input file: $input_file, cell size: $cell_size, verbose: $verbose\n";
```

`"input=s"` declares an option named `input` that takes a string (`s`) argument; `"cellsize=f"` takes a floating-point number (`f`); `"verbose"` (no suffix) is a plain on/off flag. The `\$input_file` syntax — a backslash before a scalar variable — creates a **reference** to that variable, which is how `GetOptions` is able to write the parsed value directly back into your own variables, conceptually similar in spirit to how a C function receives `&some_variable` to modify the caller's own data (Part 9 and Part 11 of the C primer). This module-based approach is generally preferable for any new macro-style script you write yourself, even though many of MB-System's original, older macros predate widespread `Getopt::Long` adoption and use manual `@ARGV` parsing instead, as shown in Section 10.1.

---

## Part 11 — Anatomy of a Real `mbm_*` Macro

### 11.1 The General Shape

Every macro in `src/macros/` follows roughly the same overall structure, which you can now read and understand section by section:

1. A **usage/help block**, printed if the script is run with no arguments or with `-h`, describing every supported option.
2. **Argument parsing**, populating a set of variables (or a hash) describing exactly what the user asked for — input files, region bounds, cell size, output filename, and any optional plotting/formatting flags.
3. **Validation**, checking that required options were actually supplied and are individually sane (existing files, positive cell sizes, well-formed bounds strings) — using exactly the `die`-based checks and regex validation from Parts 6 and 8.
4. **Derived-value computation**, working out anything the generated script will need that the user did not supply directly — default output filenames built from the input filename, computed grid dimensions from bounds and cell size (Part 7's `compute_grid_dimensions` example), a default GMT projection string.
5. **Shell script assembly**, building up the final script's text incrementally (Part 9), often as a large heredoc combined with conditional appends for optional sections.
6. **Writing and marking the script executable** (Section 9.2), followed by a final message telling the user what was written and how to run it.

### 11.2 A Guided, Simplified Reconstruction

Here is an original, simplified script written in exactly this shape, modeled on the real, documented behavior of `mbm_grid` — which generates an executable shellscript that will build a grid or mosaic from swath data [web:32] — without reproducing any of that macro's actual source text:

```perl
#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;

# --- 1. Usage block ---
sub print_usage {
    print <<"USAGE";
Usage: mysimplegrid.pl --input <datalist> --output <root> [--cellsize N] [--bounds W/E/S/N]

  --input      Input datalist file (required)
  --output     Root name for output grid file (required)
  --cellsize   Grid cell size in meters (default: 5.0)
  --bounds     Region as west/east/south/north (optional; auto-detected if omitted)
USAGE
    exit(1);
}

# --- 2. Argument parsing ---
my ($input_file, $output_root, $bounds_string);
my $cell_size = 5.0;

GetOptions(
    "input=s"    => \$input_file,
    "output=s"   => \$output_root,
    "cellsize=f" => \$cell_size,
    "bounds=s"   => \$bounds_string,
) or print_usage();

print_usage() unless (defined($input_file) && defined($output_root));

# --- 3. Validation ---
die "Error: input file '$input_file' does not exist\n" unless (-e $input_file);
die "Error: cell size must be a positive number\n" unless ($cell_size =~ /^\d+(\.\d+)?$/ && $cell_size > 0);

my ($west, $east, $south, $north);
if (defined($bounds_string)) {
    ($west, $east, $south, $north) = split(/\//, $bounds_string);
    die "Error: --bounds must be west/east/south/north\n" unless (defined($north));
}

# --- 4. Derived values ---
my $region_flag = defined($bounds_string) ? "-R$bounds_string" : "";
my $grd_file = "${output_root}.grd";

# --- 5. Shell script assembly ---
my $script_text = <<"END_SCRIPT";
#!/bin/sh
# Auto-generated by mysimplegrid.pl -- do not edit by hand
echo Running mbgrid to build $grd_file ...
mbgrid -I $input_file -A2 -E${cell_size}/${cell_size} $region_flag -O $output_root
echo Done. Output grid: $grd_file
END_SCRIPT

# --- 6. Write and mark executable ---
my $script_name = "${output_root}_grid.cmd";
open(my $fh, ">", $script_name) or die "Cannot open $script_name: $!\n";
print $fh $script_text;
close($fh);
chmod(0755, $script_name);

print "Wrote script: $script_name\n";
print "Run it with: ./$script_name\n";
```

Every piece of syntax in this script — `use strict`/`use warnings`, `GetOptions`, heredocs, regex validation, ternary-style conditional string assignment (`? "-R$bounds_string" : ""`), `open`/`print`/`close`/`chmod` — has been individually explained in Parts 2 through 10. Reading this script top to bottom should now feel like reading ordinary, comprehensible code, not unfamiliar syntax, which is exactly the goal: you are now equipped to open the real `mbm_grid` or `mbm_plot` source in `src/macros/` and recognize the same overall shape, even though the real macros are considerably larger, support far more options, and handle many more edge cases than this deliberately minimal reconstruction.

### 11.3 The Ternary Operator, Noted

Section 11.2 used `defined($bounds_string) ? "-R$bounds_string" : ""` — Perl's **ternary operator**, `condition ? value_if_true : value_if_false`, evaluating to one of two values depending on the condition, useful for exactly this kind of compact "choose one string or another" logic when assembling command-line flag fragments.

---

## Part 12 — Writing Your Own MB-System Macro

### 12.1 A Practical Checklist

When writing a new `mbm_*`-style macro of your own, work through this sequence, mirroring Part 11's anatomy:

1. Write the usage/help text first — it forces you to decide, up front, exactly what options your macro needs and what each one means.
2. Parse arguments with `Getopt::Long`, storing results in clearly named scalar variables.
3. Validate every user-supplied value immediately after parsing — file existence, numeric well-formedness, required-option presence — using `die` with a clear, actionable message for each failure.
4. Compute any derived values your generated script will need.
5. Assemble the generated script's text as a heredoc, with `if` blocks appending optional sections.
6. Write the file, `chmod` it executable, and print a clear final message telling the user what was produced and how to run it.

### 12.2 A Worked Extension Exercise

Extend Section 11.2's script so that it also optionally generates a plotting step, using exactly the conditional-append technique from Section 9.4:

```perl
my $make_plot = 0;
GetOptions(
    # ... existing options ...
    "plot" => \$make_plot,
) or print_usage();

# ... existing script_text assembly ...

if ($make_plot) {
    $script_text .= "mbm_grdplot -I $grd_file -G1 -C1 -V\n";
}
```

This single addition — a new boolean flag, plus one conditionally appended line of generated shell text — is precisely the kind of small, well-scoped, testable change that mirrors how real MB-System macros have grown new capabilities over time: one new option, one new validation check if needed, one new conditionally-included block of generated script text.

---

## Part 13 — Debugging and Common Pitfalls

### 13.1 Always Run With `use strict; use warnings;`

Revisit Part 2: these two lines catch an enormous fraction of real Perl mistakes before you ever see confusing runtime behavior — typos in variable names, use of an uninitialized value, and dozens of other common errors, all reported with a specific line number.

### 13.2 `eq`/`ne` vs. `==`/`!=`

Covered in Section 10.1: string comparisons need `eq`/`ne`/`lt`/`gt`; numeric comparisons need `==`/`!=`/`<`/`>`. Using the wrong pair does not usually produce an error — it silently does something other than what you intended, since Perl freely converts between numbers and strings, which makes this class of mistake particularly easy to miss without close review.

### 13.3 Quoting: Single vs. Double, Inside Generated Shell Text

The most MB-System-macro-specific pitfall: when your Perl script's generated shell-script text itself needs to contain a literal `$` (for the *shell* to interpret later — a shell variable, or a positional parameter like `$1`), you must prevent *Perl's own* double-quote interpolation from consuming it first, either by using single quotes for that portion of the text or by escaping it as `\$` inside a double-quoted string or heredoc:

```perl
my $script_text = <<'END_SCRIPT';
#!/bin/sh
# Here, $1 refers to THIS SHELL SCRIPT's first argument, not a Perl variable,
# because this heredoc uses single quotes around its terminator (<<'END_SCRIPT')
echo "Processing file: $1"
END_SCRIPT
```

Note the single quotes around `'END_SCRIPT'` in the heredoc's opening line — this switches the entire heredoc to behave like a single-quoted string, disabling Perl interpolation entirely, which is exactly what you want when the generated text is full of shell-native `$` references rather than Perl variables. Mixing this up — accidentally letting Perl try to interpolate a shell variable reference meant for the generated script — is one of the most common real bugs when writing script-generating macros, and recognizing which heredoc quoting style is in use is the first thing to check when a generated script's output looks wrong.

### 13.4 Testing a Macro Safely

Before trusting a new or modified macro against real survey data, run it against a small, disposable test datalist and inspect the *generated* script's text directly (`cat script_name.cmd`) before ever executing it — exactly the same evidence-driven, inspect-before-trust discipline you already apply when validating output from any new data-processing tool.

### 13.5 Perl's `-c` (Syntax-Check) Flag

```sh
perl -c mysimplegrid.pl
```

This checks the script for syntax errors without actually running it — a fast first check after any edit, before testing actual behavior.

---

## Part 14 — Where to Go Next

You now have a complete, working foundation in Perl as it is actually used inside MB-System: scalars, arrays, and hashes; every major control-flow construct; subroutines and multiple return values; regular expressions for matching, validating, and substituting text; file I/O and heredocs for generating shell-script content; command-line parsing both manually and via `Getopt::Long`; and a full, guided reconstruction of a real macro's overall shape and conventions.

From here, the most productive next step is to open a real macro directly — `src/macros/mbm_grid` or `src/macros/mbm_plot` are the natural starting points, given how thoroughly this primer has walked through their conceptual shape — and read it start to finish, expecting to recognize nearly everything syntactically, with any remaining unfamiliarity limited to domain-specific detail (which exact GMT flags a given plotting mode requires, which MB-System tool options a given grid mode assembles) rather than the Perl language itself. A well-scoped first real contribution might be adding one new optional flag to an existing macro, following exactly the checklist in Part 12, and validating the change by inspecting its generated shell script's text before ever running it against real data.
