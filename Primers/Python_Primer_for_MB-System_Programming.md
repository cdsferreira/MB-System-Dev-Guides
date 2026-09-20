# Python Primer for MB-System Programming

*A beginner-to-intermediate introduction to Python, taught through MB-System's newer, Python-based macros.*

---

## How to Use This Primer

This primer assumes no prior Python experience, though it assumes the general programming comfort you already bring from C, Perl, and marine-data tooling work. Every concept is explained from first principles, and every non-trivial example is built around MB-System's real conventions for its newer macros rather than generic Python exercises.

MB-System is a collaborative effort between the Monterey Bay Aquarium Research Institute (MBARI), the Center for Marine Environmental Sciences (MARUM) at Universität Bremen, and the Center for Coastal and Ocean Mapping (CCOM) at the University of New Hampshire. The companion *Perl Primer for MB-System Programming* covered the `mbm_*` macro family's historical implementation language. Perl remains dominant for the older shell-script-generating macros (`mbm_grid`, `mbm_plot`, `mbm_grdplot`), but **newer MB-System macros are increasingly written in Python** instead [web:31]. The clearest real example is `mbm_phins2fnv`, a Python-based macro that converts processed Phins inertial navigation system (INS) logs into MB-System's `.fnv` navigation format, using the `pandas` library to parse space-separated INS log columns and map them onto the fields MB-System's navigation-processing tools expect [web:31].

This is not a coincidence of author preference. Python earns its place in the newer macros specifically because of its data-science ecosystem — `pandas` for tabular, column-oriented data (exactly the shape of an INS log, a `.fnv` file, or a navigation CSV) and `numpy` for fast numeric array operations (exactly the shape of a beam array or a time series of attitude values) — capabilities Perl simply does not have built in, and which are increasingly the natural fit as MB-System's macro layer takes on more numeric data-transformation work (format conversion, coordinate transforms, navigation post-processing) rather than purely generating shell-script text. You already work with exactly this kind of navigation and INS data in your own AUV/ROV work, which makes this primer's central example unusually directly relevant.

---

## Table of Contents

- Part 1 — Why Python for the Newer MB-System Macros
- Part 2 — Setting Up a Python Environment
- Part 3 — Python Basics: Variables and Types
- Part 4 — Core Data Structures: Lists, Tuples, and Dictionaries
- Part 5 — Control Flow
- Part 6 — Functions
- Part 7 — Modules and Imports
- Part 8 — Strings and Text Processing
- Part 9 — File I/O
- Part 10 — Introduction to `pandas` for Navigation and Log Data
- Part 11 — Introduction to `numpy` for Numeric Arrays
- Part 12 — Command-Line Argument Parsing with `argparse`
- Part 13 — Anatomy of a Real Python Macro: `mbm_phins2fnv`
- Part 14 — Writing Your Own Python-Based Macro
- Part 15 — Debugging and Common Pitfalls
- Part 16 — Where to Go Next

---

## Part 1 — Why Python for the Newer MB-System Macros

### 1.1 Two Different Jobs, Two Different Languages

The Perl-based `mbm_*` macros (`mbm_grid`, `mbm_plot`, and similar) are fundamentally **script generators**: they parse a handful of options and write out a shell script that later calls other programs. Their core job is text templating, which is Perl's original strength. The newer Python-based macros, by contrast, are increasingly **data transformers**: `mbm_phins2fnv` does not generate a shell script at all — it directly reads a navigation log file, restructures its columns, and writes out a new file in MB-System's own `.fnv` navigation format [web:31]. That is a data-processing task, not a text-templating task, and Python's ecosystem — specifically `pandas` — is a dramatically better fit for it than Perl's string-and-regex toolkit would be.

### 1.2 What `pandas` and `numpy` Actually Add

`pandas` provides a **DataFrame**: an in-memory table, with named columns and an index, that can be read directly from a delimited text file, filtered, transformed column-by-column, and written back out — in a handful of lines of code that would require substantially more manual parsing logic in Perl or C. `numpy` provides fast, vectorized numeric arrays — operations like "convert this entire column of angles from degrees to radians" or "compute the difference between two entire columns" happen as a single expression operating on the whole array at once, without an explicit loop. For a task like converting an INS log's columns into `.fnv` format, or computing derived navigation quantities from raw INS output, this is exactly the right tool.

### 1.3 What You Will Be Able to Do After This Primer

By the end, you will be able to read `src/macros/mbm_phins2fnv` and understand exactly how it parses its input, restructures the data with `pandas`, and writes the `.fnv` output — and you will be able to write a small, original Python macro of your own, in the same style, for a comparable navigation- or log-conversion task.

---

## Part 2 — Setting Up a Python Environment

### 2.1 Checking for Python

macOS and most Linux distributions ship Python 3 pre-installed, but always confirm the version, since MB-System's Python macros target Python 3 specifically (Python 2 reached end-of-life in January 2020 and should not be used for any new work):

```sh
python3 --version
```

### 2.2 Virtual Environments

A **virtual environment** is an isolated, self-contained set of installed Python packages, separate from your system-wide Python installation — important because different projects can require different, sometimes conflicting, versions of the same package. Create and activate one for this primer's exercises:

```sh
python3 -m venv ~/mbsystem-python-primer/venv
source ~/mbsystem-python-primer/venv/bin/activate
```

Once activated, your shell prompt typically changes to show the environment's name, and any `pip install` you run from here on installs only into this isolated environment, not system-wide.

### 2.3 Installing `pandas` and `numpy`

```sh
pip install pandas numpy
```

`pip` is Python's standard package installer, pulling packages from the Python Package Index (PyPI). Confirm the install:

```sh
python3 -c "import pandas; import numpy; print(pandas.__version__, numpy.__version__)"
```

### 2.4 Running a Python Script

```sh
python3 myscript.py arg1 arg2
```

Or, with a shebang line and execute permission, exactly as with the Perl macros:

```python
#!/usr/bin/env python3
```

```sh
chmod +x myscript.py
./myscript.py arg1 arg2
```

### 2.5 A First Python Program

```python
#!/usr/bin/env python3
"""A minimal, MB-System-flavored first Python program."""

def main():
    print("Hello from an MB-System-style Python macro")

if __name__ == "__main__":
    main()
```

The triple-quoted string at the top is a **docstring** — a conventional way to document what a module or function does, distinct from an ordinary `#` comment, and Python tooling can extract and display docstrings automatically (for `help()` output, for instance). The `if __name__ == "__main__":` block is a near-universal Python idiom: `__name__` is a special built-in variable that Python sets to `"__main__"` only when a file is run *directly* (as opposed to being `import`ed by some other script), so this pattern lets a file define reusable functions that can be safely imported elsewhere, while still doing something useful when run on its own — exactly the shape you will see in real MB-System Python macros.

---

## Part 3 — Python Basics: Variables and Types

### 3.1 Variables Need No Declared Type

Unlike C, Python variables carry no fixed type declaration at all — a name is simply bound to a value, and that same name can be rebound to a value of a completely different type later (though doing so deliberately, mid-script, is poor style and best avoided):

```python
survey_name = "Bremen_Bay_2026"
cell_size = 5.0
nfiles = 12
is_verbose = True
```

### 3.2 The Basic Built-In Types

| Type | Example | Notes |
|---|---|---|
| `int` | `42` | arbitrary precision — Python `int`s do not overflow the way C's fixed-width integers can |
| `float` | `5.0`, `-8.90` | double-precision, same underlying representation as C's `double` |
| `str` | `"EM710"` | text; single and double quotes are interchangeable in Python |
| `bool` | `True`, `False` | capitalized; Python's actual boolean type, unlike C |
| `NoneType` | `None` | Python's equivalent of "no value," roughly analogous to C's `NULL` |

```python
beam_count = 24
latitude_deg = 53.0793
sensor_name = "EM710"
is_flagged = False
grid_bounds = None   # not yet computed
```

### 3.3 Checking a Value's Type

```python
print(type(beam_count))      # <class 'int'>
print(type(latitude_deg))    # <class 'float'>
print(isinstance(beam_count, int))   # True
```

### 3.4 Arithmetic Operators, and Why Integer Division Is Not a Trap in Python 3

```python
total_beams = 7
good_beams = 4

fraction = good_beams / total_beams       # "/" ALWAYS produces a float in Python 3: 0.5714...
whole_division = good_beams // total_beams   # "//" is explicit integer (floor) division: 0
remainder = good_beams % total_beams      # modulo: 4
```

This is a genuinely important, deliberate difference from C, which you should note carefully given how much time the companion C primer spent warning about integer-division truncation: in Python 3, the plain `/` operator **always** produces a floating-point result, even when both operands are integers. If you specifically want C-style truncating integer division, Python requires the separate `//` operator to say so explicitly. This single language design choice eliminates an entire, extremely common class of C bugs, at the cost of requiring you to actively choose `//` on the rare occasions you genuinely want truncation.

### 3.5 f-strings: Modern Python String Formatting

```python
depth_m = 23.457
sensor = "EM710"

message = f"Sensor {sensor} recorded depth {depth_m:.3f} m"
print(message)   # Sensor EM710 recorded depth 23.457 m
```

An **f-string** — a string literal prefixed with `f` — lets you embed Python expressions directly inside `{ }` placeholders, with optional formatting specifiers after a colon (`:.3f` here means "format as a float with 3 decimal places," directly analogous to C's `%.3f` or Perl's sprintf-style formatting). f-strings are the standard, idiomatic way to build formatted output and messages in modern Python (Python 3.6 and later), and you will use them constantly.

---

## Part 4 — Core Data Structures: Lists, Tuples, and Dictionaries

### 4.1 Lists: Ordered, Mutable Sequences

A **list** is Python's closest equivalent to an array, but unlike a C array, it can grow, shrink, and hold values of mixed types (though for clarity, MB-System-style code generally keeps a given list's elements uniformly typed):

```python
input_files = ["EM710_0001.mb88", "EM710_0002.mb88", "EM710_0003.mb88"]

print(input_files[0])       # first element: "EM710_0001.mb88"
print(len(input_files))     # length: 3
input_files.append("EM710_0004.mb88")   # grow the list
```

Indexing starts at 0, exactly as in C and Perl. Python additionally supports **negative indices**, counting from the end: `input_files[-1]` is the last element, without needing to know or compute the list's length first.

### 4.2 List Slicing

```python
first_two = input_files[0:2]    # elements at index 0 and 1 (end index is exclusive)
last_two  = input_files[-2:]    # the last two elements
```

Slicing — `list[start:stop]` — is used constantly for extracting subsets of a longer sequence (a range of pings, a subset of columns) without writing an explicit loop.

### 4.3 List Comprehensions

A list comprehension builds a new list from an existing one in a single, compact expression:

```python
depths_m = [12.1, 12.4, 11.9, 250.0, 12.6, 12.2, -1.0, 12.5]

good_depths = [d for d in depths_m if 0.0 < d <= 200.0]
print(good_depths)   # [12.1, 12.4, 11.9, 12.6, 12.2, 12.5]
```

`[d for d in depths_m if 0.0 < d <= 200.0]` reads naturally left to right: "for each `d` in `depths_m`, keep it if `0.0 < d <= 200.0`, and collect the results into a new list." This is a genuinely idiomatic, widely used Python pattern, directly replacing the explicit `for` loop with `if`/`continue` you saw for the equivalent operation in both the C and Perl primers, and it is worth becoming comfortable reading and writing, since real Python code (MB-System's included) uses it heavily.

### 4.4 Tuples: Ordered, Immutable Sequences

A **tuple** looks like a list but cannot be modified after creation — used for small, fixed groupings of values, especially when returning multiple values from a function:

```python
bounds = (-8.90, -8.50, 53.00, 53.40)   # (west, east, south, north)
west, east, south, north = bounds        # "unpacking" a tuple into separate variables

print(f"West: {west}, East: {east}")
```

This tuple-unpacking assignment is Python's equivalent of Perl's `my ($west, $east, ...) = @bounds;` and of the multiple-return-value pattern in the C primer's Part 9 — but here it needs no pointers or special return-list syntax at all; a function simply returns a tuple, and the caller unpacks it, exactly as shown.

### 4.5 Dictionaries: Key-Value Mappings

A **dictionary** (`dict`) is Python's equivalent of a Perl hash: an unordered (technically, insertion-ordered since Python 3.7) collection of key-value pairs:

```python
options = {
    "cellsize": 5.0,
    "boundaries": "-8.9/-8.5/53.0/53.4",
    "outfile": "grid_out",
}

print(options["cellsize"])       # 5.0
options["projection"] = "u32N"   # add a new key
```

### 4.6 Checking for a Key, and Iterating

```python
if "projection" in options:
    print(f"Projection specified: {options['projection']}")

for key, value in options.items():
    print(f"{key} => {value}")
```

`in` checks membership directly and reads naturally; `.items()` gives you both the key and the value together in one iteration step, which is more direct than the separate `keys %options` iteration style from the Perl primer.

---

## Part 5 — Control Flow

### 5.1 `if` / `elif` / `else`

```python
if cell_size <= 0:
    raise ValueError("cell size must be positive")
elif cell_size < 1.0:
    print("Warning: very fine cell size, this may take a long time")
else:
    print("Cell size looks reasonable")
```

Note Python's use of **indentation** itself, rather than curly braces, to mark a block's extent — this is not merely a style convention, as it is in C, but the actual, load-bearing syntax that defines where a block begins and ends. Consistent indentation (4 spaces is the near-universal convention, per Python's official style guide, PEP 8) is mandatory, not optional, and mixing tabs and spaces inconsistently within one file is a genuine syntax error.

### 5.2 `raise` and Exceptions

`raise ValueError("...")` above is Python's mechanism for signaling an error — conceptually similar to Perl's `die` or a C function's `MB_FAILURE` return plus error code, but implemented through a distinct, structured **exception** mechanism rather than a plain return value. An unhandled exception terminates the script with a traceback showing exactly where it occurred; a handled one can be caught and responded to:

```python
try:
    cell_size = float(user_input)
except ValueError:
    print("Error: cell size must be a valid number")
    cell_size = 5.0   # fall back to a default
```

`try`/`except` lets a script attempt an operation that might fail (here, converting a string to a `float`, which raises `ValueError` if the string is not a valid number) and respond gracefully rather than crashing outright — a pattern used throughout real-world Python data-processing scripts, including when reading and parsing values from an external log file whose exact formatting cannot always be perfectly guaranteed in advance.

### 5.3 Truthiness in Python

Similar in spirit to both C and Perl: `0`, `0.0`, `""` (empty string), `[]` (empty list), `{}` (empty dict), and `None` are all treated as **false**; everything else is **true**. `if my_list:` is a common, idiomatic way to check "is this list non-empty," directly analogous to a C pointer's implicit truthiness check, but here applied to a container rather than an address.

### 5.4 Loops: `for` and `while`

```python
for file in input_files:
    print(f"Processing {file}")

i = 0
while i < len(input_files):
    print(f"File {i}: {input_files[i]}")
    i += 1
```

Python's `for` loop is fundamentally a `foreach`-style loop, iterating directly over a sequence's elements (exactly like Perl's `foreach`), rather than a C-style indexed counter loop by default. When you genuinely need the index alongside each element, use `enumerate`:

```python
for i, file in enumerate(input_files):
    print(f"File {i}: {file}")
```

### 5.5 `break` and `continue`

Identical in meaning to their C and Perl counterparts: `break` exits the loop immediately; `continue` skips to the next iteration.

```python
for i, depth in enumerate(depths_m):
    if depth <= 0.0 or depth > 200.0:
        print(f"Beam {i} flagged as bad: {depth:.2f} m")
        continue
    # normal processing continues here
```

---

## Part 6 — Functions

### 6.1 Defining and Calling

```python
def compute_grid_dimensions(west, east, south, north, cellsize):
    ncols = int((east - west) / cellsize) + 1
    nrows = int((north - south) / cellsize) + 1
    return ncols, nrows

ncols, nrows = compute_grid_dimensions(-8.90, -8.50, 53.00, 53.40, 0.005)
print(f"Grid will be {ncols} columns by {nrows} rows")
```

`return ncols, nrows` is actually returning a single tuple `(ncols, nrows)`, which the caller then unpacks — exactly the mechanism from Section 4.4, now applied at a function boundary. This is Python's clean, direct equivalent of both the C primer's pointer-based multiple-return-value pattern and the Perl primer's list-returning subroutines, with no special syntax needed on either side beyond an ordinary tuple.

### 6.2 Default Arguments

```python
def build_plot_command(input_file, output_file, projection="M6i"):
    return f"mbm_plot -I {input_file} -O {output_file} -Jm{projection}"

cmd1 = build_plot_command("swath1.mb88", "swath1_plot")             # uses default projection
cmd2 = build_plot_command("swath1.mb88", "swath1_plot", "u32N")     # overrides it
```

### 6.3 Keyword Arguments

Any function call can pass arguments by name rather than position, improving readability at the call site, especially for functions with several optional parameters:

```python
cmd = build_plot_command(input_file="swath1.mb88", output_file="swath1_plot", projection="u32N")
```

### 6.4 Type Hints (Recommended Style)

Modern Python supports optional **type hints** — annotations describing a function's expected parameter and return types, purely for documentation and tooling purposes (unlike C, Python does not enforce these at runtime by default, but editors and static-analysis tools use them to catch mistakes before you ever run the code):

```python
def compute_grid_dimensions(west: float, east: float, south: float,
                             north: float, cellsize: float) -> tuple[int, int]:
    ncols = int((east - west) / cellsize) + 1
    nrows = int((north - south) / cellsize) + 1
    return ncols, nrows
```

Using type hints in any new Python macro code you write is good practice — it documents intent clearly for future readers, without changing the function's actual runtime behavior at all.

---

## Part 7 — Modules and Imports

### 7.1 The Standard Library

Python ships with a large **standard library** — modules that come pre-installed with every Python installation, requiring no `pip install`. Import what you need at the top of a script:

```python
import sys
import os
import math

print(sys.argv)               # command-line arguments, Python's equivalent of Perl's @ARGV / C's argv
print(os.path.exists("file.txt"))   # file existence check, like Perl's -e or C's stat-based checks
print(math.pi)
```

### 7.2 Third-Party Packages

`pandas` and `numpy`, installed via `pip` in Part 2, are **third-party packages** — not part of the standard library, but installed separately and then imported exactly the same way:

```python
import pandas as pd
import numpy as np
```

`as pd` and `as np` are **aliases** — conventional, near-universal abbreviations used throughout the entire Python data-science ecosystem; virtually every piece of real-world code using these two libraries imports them under exactly these two names, and you should follow this convention in your own code for consistency with everything else you will read.

### 7.3 Importing Specific Names

```python
from math import pi, cos, sin

angle_rad = 32.5 * pi / 180.0
depth_m = 45.2 * cos(angle_rad)
```

`from module import name1, name2` brings specific names directly into your script's namespace, letting you write `cos(...)` instead of `math.cos(...)`. Both styles are common; importing the whole module (`import math`) and qualifying every use (`math.cos(...)`) is generally considered clearer for larger scripts, since it always makes obvious which module a given function came from.

---

## Part 8 — Strings and Text Processing

### 8.1 String Basics

```python
filename = "EM710_survey_0042.mb88"

print(len(filename))          # length
print(filename.upper())       # "EM710_SURVEY_0042.MB88"
print(filename.split("_"))    # ['EM710', 'survey', '0042.mb88']
print(filename.replace("EM710", "EM712"))
```

Strings in Python are objects with built-in **methods** — functions called with `.method_name()` syntax directly on the string itself, rather than passed as an argument to a separate function (as in C's `strlen(s)` or Perl's `length($s)`). This method-call style is pervasive throughout Python and worth getting comfortable with immediately.

### 8.2 Splitting and Joining

```python
parts = filename.split("_")           # ['EM710', 'survey', '0042.mb88']
rejoined = "-".join(parts)            # 'EM710-survey-0042.mb88'
```

Directly analogous to Perl's `split`/`join`, but called as methods rather than standalone functions.

### 8.3 Checking String Contents

```python
if filename.endswith(".mb88"):
    print("This looks like an MB-System format 88 file")

if "survey" in filename:
    print("Filename contains 'survey'")
```

### 8.4 Regular Expressions (`re` module)

Python's regex support lives in the standard library's `re` module, and its pattern syntax is very close to Perl's (Part 8 of the Perl primer), though the calling style is different — Python has no built-in `=~` operator; instead you call functions from `re`:

```python
import re

match = re.search(r"\.mb(\d+)$", filename)
if match:
    format_code = match.group(1)
    print(f"Detected MB-System format code: {format_code}")
```

`r"\.mb(\d+)$"` is a **raw string** (the `r` prefix) — it tells Python not to process backslash escape sequences the way it normally would in a string literal, which matters enormously for regex patterns, since they are full of literal backslashes (`\d`, `\.`) that must reach the regex engine unmodified. **Always use raw strings for regex patterns in Python**; forgetting the `r` prefix is a common source of subtly broken patterns, since Python's own string-escaping rules would otherwise interfere with the regex engine's separate escaping rules. `re.search` returns a match object (or `None` if no match was found) rather than automatically populating a variable like Perl's `$1`; you retrieve captured groups explicitly with `.group(1)`, `.group(2)`, and so on.

### 8.5 Substitution

```python
template = "mbm_grid -I INPUTFILE -O OUTPUTFILE -A CELLSIZE"
result = re.sub("INPUTFILE", "datalist.mb-1", template)
```

`re.sub(pattern, replacement, string)` mirrors Perl's `s/pattern/replacement/`, but as an explicit function call rather than an in-place operator on the string.

---

## Part 9 — File I/O

### 9.1 Reading a Text File

```python
with open("survey_log.txt", "r") as f:
    for line in f:
        print(line.strip())   # strip() removes the trailing newline and any surrounding whitespace
```

`with open(...) as f:` is Python's standard idiom for working with files: it automatically closes the file when the `with` block ends, even if an error occurs partway through — Python's equivalent of C's manual `fclose` or Perl's manual `close`, but guaranteed to run via the `with` block's structure rather than relying on you to remember it, which eliminates an entire class of "forgot to close the file" bugs. Iterating directly over an open file object (`for line in f:`) yields one line at a time, automatically, without any explicit end-of-file check.

### 9.2 Writing a Text File

```python
with open("output.fnv", "w") as f:
    f.write("2026 09 20 12 00 00.000 8.8017 53.0793 0.0 0.0 0.0\n")
    f.write("2026 09 20 12 00 01.000 8.8018 53.0794 0.0 0.0 0.0\n")
```

### 9.3 Reading Delimited Data Manually

Before introducing `pandas` in Part 10, it is worth seeing what parsing a delimited log file manually looks like, since it clarifies exactly what `pandas` is doing for you automatically underneath:

```python
records = []

with open("phins_log.txt", "r") as f:
    header = f.readline()   # skip/inspect the header line
    for line in f:
        fields = line.split()   # split on any whitespace
        time_str, lat_str, lon_str, heading_str = fields[0], fields[1], fields[2], fields[3]
        records.append({
            "time": time_str,
            "latitude": float(lat_str),
            "longitude": float(lon_str),
            "heading": float(heading_str),
        })

print(f"Read {len(records)} records")
```

This loop — split each line, convert each field to the right type, collect into a list of dictionaries — is exactly the kind of manual, repetitive parsing code that `pandas` replaces with a single function call, as the next part demonstrates directly on the same kind of data.

---

## Part 10 — Introduction to `pandas` for Navigation and Log Data

### 10.1 Reading a Delimited File Into a DataFrame

The single most common `pandas` operation for exactly the kind of INS/navigation log data `mbm_phins2fnv` processes [web:31]:

```python
import pandas as pd

df = pd.read_csv(
    "phins_log.txt",
    sep=r"\s+",              # one or more whitespace characters as the delimiter
    header=None,             # this INS log has no header row of column names
    names=["date", "time", "latitude", "longitude", "heading", "roll", "pitch", "heave"],
)

print(df.head())     # preview the first 5 rows
print(df.shape)      # (number_of_rows, number_of_columns)
print(df.dtypes)     # the detected data type of each column
```

`pd.read_csv`, despite its name, reads any delimiter-separated text file, not only comma-separated ones — `sep=r"\s+"` (a raw-string regex, exactly as introduced in Section 8.4) tells it to treat any run of whitespace as a field separator, which is exactly the format of many space-separated INS and navigation log files. `header=None` plus an explicit `names=[...]` list is used when the source file has no column-name header row of its own, as is common for raw sensor logs — you supply the column names yourself, based on knowing the log format's fixed field order.

### 10.2 Selecting and Transforming Columns

```python
latitudes = df["latitude"]            # a single column, returned as a pandas Series
subset = df[["latitude", "longitude"]]   # multiple columns, returned as a smaller DataFrame

df["heading_rad"] = df["heading"] * 3.14159265358979 / 180.0   # new column, computed from an existing one
```

Notice there is no explicit loop anywhere in `df["heading_rad"] = df["heading"] * ... / 180.0` — this single line converts an *entire column* of values from degrees to radians in one vectorized operation, applied to every row simultaneously. This is precisely the `numpy`-powered efficiency mentioned in Section 1.2: internally, `pandas` columns are built on `numpy` arrays, and arithmetic on them operates on the whole array at once rather than element-by-element in an explicit Python loop, which is both far more concise to write and substantially faster to execute than the equivalent hand-written loop.

### 10.3 Filtering Rows

```python
good_rows = df[df["heading"].between(0, 360)]
bad_rows  = df[~df["heading"].between(0, 360)]   # ~ negates a boolean condition

print(f"Good rows: {len(good_rows)}, bad rows: {len(bad_rows)}")
```

`df["heading"].between(0, 360)` produces a boolean Series (`True`/`False` for every row); indexing the DataFrame with that boolean Series, `df[...]`, keeps only the rows where the condition was `True` — this "boolean masking" idiom is the standard `pandas` way of filtering data, replacing the explicit `if`/`continue` loop pattern used for the same purpose in the C and Perl primers.

### 10.4 Combining Date and Time Columns Into a Single Timestamp

INS logs frequently store date and time in separate columns, which need combining into a single value before further processing — exactly the kind of transformation `mbm_phins2fnv` performs when mapping raw INS columns onto MB-System's expected navigation fields [web:31]:

```python
df["timestamp"] = pd.to_datetime(df["date"] + " " + df["time"], format="%Y%m%d %H%M%S.%f")
```

`pd.to_datetime` parses a column of date/time strings into `pandas`'s dedicated datetime type, given a `format` string describing exactly how the source strings are laid out (`%Y` four-digit year, `%m` month, `%d` day, `%H%M%S.%f` hours-minutes-seconds-with-fractional-seconds here, concatenated with no separators, matching a plausible raw INS log convention) — the same `strftime`-style format codes used across many languages, including Python's own standard-library `datetime` module and, not coincidentally, closely related conventions in C's `strftime`.

### 10.5 Writing the Result Out — Building a `.fnv`-Style File

MB-System's `.fnv` ("fast navigation") format is a simple space- or tab-delimited text file, one navigation fix per line. Producing one from a cleaned-up DataFrame is a natural final step:

```python
output_df = pd.DataFrame({
    "year":    df["timestamp"].dt.year,
    "month":   df["timestamp"].dt.month,
    "day":     df["timestamp"].dt.day,
    "hour":    df["timestamp"].dt.hour,
    "minute":  df["timestamp"].dt.minute,
    "second":  df["timestamp"].dt.second + df["timestamp"].dt.microsecond / 1e6,
    "longitude": df["longitude"],
    "latitude":  df["latitude"],
    "heading":   df["heading"],
})

output_df.to_csv("output.fnv", sep=" ", header=False, index=False, float_format="%.6f")
```

`.dt.year`, `.dt.month`, and so on are **accessor** properties available on any `pandas` datetime column, extracting each individual date/time component as its own Series — again, applied across the entire column at once. `to_csv(..., sep=" ", header=False, index=False)` writes the DataFrame out as a plain space-separated text file, deliberately omitting both a header row (`header=False`) and `pandas`'s own automatic row-number index column (`index=False`), which is not part of the actual `.fnv` format and would otherwise be written by default. `float_format="%.6f"` controls the decimal precision of the numeric output — directly analogous to `printf`'s `%.6f` in C or an f-string's `:.6f` from Section 3.5, applied here to every floating-point value the DataFrame writes out.

### 10.6 Basic Descriptive Statistics

```python
print(df["heading"].mean())
print(df["heading"].std())
print(df["heading"].min(), df["heading"].max())
print(df.describe())   # a full summary table: count, mean, std, min, quartiles, max, for every numeric column
```

`.describe()` in particular is an extremely convenient first diagnostic step whenever you load a new log file for the first time — a single call that immediately surfaces obviously wrong values (an impossible latitude, a wildly out-of-range heading) worth investigating before any further processing.

---

## Part 11 — Introduction to `numpy` for Numeric Arrays

### 11.1 Why `numpy`, Alongside `pandas`

`pandas` is built on top of `numpy`, and for pure numeric-array work without the need for a labeled, column-oriented table structure — for instance, working directly with a beam array from a single ping, rather than a full navigation log — `numpy` alone is often the more direct tool.

### 11.2 Creating and Operating on Arrays

```python
import numpy as np

depths_m = np.array([12.1, 12.4, 11.9, 250.0, 12.6, 12.2, -1.0, 12.5])

good_mask = (depths_m > 0.0) & (depths_m <= 200.0)   # a boolean array, same length as depths_m
good_depths = depths_m[good_mask]

print(good_depths)
print(good_depths.mean())
print(good_depths.std())
```

`&` here is `numpy`'s element-wise boolean AND (not Python's plain `and`, which does not work element-by-element on arrays); this "build a boolean mask, then index with it" pattern is precisely the same idea as `pandas`'s boolean-masking filter from Section 10.3, since `pandas` columns are `numpy` arrays underneath.

### 11.3 Vectorized Geometry, Revisited

The beam-geometry calculation from both companion primers, now vectorized across an entire array of beams at once, with no explicit loop:

```python
angles_deg = np.array([32.5, 28.1, 15.0, -10.2, -25.7])
ranges_m   = np.array([45.2, 40.1, 30.5, 22.3, 38.9])

angles_rad = np.radians(angles_deg)
depths_m        = ranges_m * np.cos(angles_rad)
across_track_m  = ranges_m * np.sin(angles_rad)

for a, d, x in zip(angles_deg, depths_m, across_track_m):
    print(f"angle={a:6.1f} deg -> depth={d:7.3f} m, across-track={x:8.3f} m")
```

`np.radians`, `np.cos`, and `np.sin` all operate on the entire input array at once, producing a same-length output array — the `numpy` equivalent of the single-value `math.cos`/`math.sin` calls from Section 7.3, scaled up to operate on many beams simultaneously, exactly mirroring how a real per-ping beam-processing step would be written in Python rather than as an explicit per-beam loop.

---

## Part 12 — Command-Line Argument Parsing with `argparse`

### 12.1 A Basic `argparse` Setup

Python's standard-library `argparse` module is the direct equivalent of Perl's `Getopt::Long` (Part 10 of the Perl primer), and is the standard, idiomatic way to parse command-line options in any real Python script, including MB-System's Python-based macros:

```python
#!/usr/bin/env python3
import argparse

def main():
    parser = argparse.ArgumentParser(description="Convert a Phins INS log to MB-System .fnv format")
    parser.add_argument("--input", required=True, help="Input Phins log file")
    parser.add_argument("--output", required=True, help="Output .fnv file")
    parser.add_argument("--verbose", action="store_true", help="Print extra diagnostic output")

    args = parser.parse_args()

    print(f"Input: {args.input}")
    print(f"Output: {args.output}")
    print(f"Verbose: {args.verbose}")

if __name__ == "__main__":
    main()
```

`add_argument("--input", required=True, help="...")` declares a required, string-valued option; `action="store_true"` declares a plain on/off flag (present or absent, taking no value of its own), directly analogous to Perl's bare `"verbose"` flag in `Getopt::Long`. `parser.parse_args()` does the actual parsing, and — critically, as a significant convenience over both C and Perl — **automatically generates a complete, correctly formatted `--help` message** from the descriptions you supplied, without any additional code:

```sh
./myscript.py --help
```

### 12.2 Typed and Optional Arguments

```python
parser.add_argument("--cellsize", type=float, default=5.0, help="Grid cell size in meters")
parser.add_argument("--bounds", type=str, default=None, help="Region as west/east/south/north")
```

`type=float` tells `argparse` to convert the raw command-line string automatically and raise a clear, well-formatted error itself if the user supplies something that cannot be parsed as a float — validation that would otherwise require a manual regex check, as shown in both companion primers' Perl and C equivalents.

---

## Part 13 — Anatomy of a Real Python Macro: `mbm_phins2fnv`

### 13.1 What This Macro Actually Does

`mbm_phins2fnv` is a real, current MB-System macro: a Python script that converts processed Phins INS data into MB-System's `.fnv` navigation format, using `pandas` to parse the space-separated INS log columns and map them onto the fields MB-System's navigation-processing tools expect [web:31]. Unlike the Perl macros covered in the companion primer, it does not generate a shell script at all — it performs the actual data transformation directly, in Python, end to end.

### 13.2 A Guided, Simplified Reconstruction

Here is an original, simplified script capturing this same overall shape — argument parsing, reading the INS log with `pandas`, restructuring columns, and writing `.fnv` output — combining every technique from Parts 10 through 12, without reproducing any of the real macro's actual source text:

```python
#!/usr/bin/env python3
"""mysimplephins2fnv.py -- a simplified, original reconstruction of the
general approach used by MB-System's mbm_phins2fnv macro."""

import argparse
import sys
import pandas as pd


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert a Phins-style INS log to MB-System .fnv format"
    )
    parser.add_argument("--input", required=True, help="Input INS log file")
    parser.add_argument("--output", required=True, help="Output .fnv file")
    parser.add_argument("--verbose", action="store_true", help="Print diagnostic output")
    return parser.parse_args()


def load_ins_log(filename, verbose=False):
    """Read a space-separated INS log into a pandas DataFrame."""
    df = pd.read_csv(
        filename,
        sep=r"\s+",
        header=None,
        names=["date", "time", "latitude", "longitude", "heading", "roll", "pitch", "heave"],
    )
    if verbose:
        print(f"Read {len(df)} rows from {filename}", file=sys.stderr)
        print(df.describe(), file=sys.stderr)
    return df


def validate_ins_log(df):
    """Basic sanity checks before trusting the data."""
    if df["latitude"].abs().max() > 90.0:
        raise ValueError("Found a latitude value outside the valid range [-90, 90]")
    if df["longitude"].abs().max() > 180.0:
        raise ValueError("Found a longitude value outside the valid range [-180, 180]")
    if df.isnull().any().any():
        raise ValueError("Found missing/unparseable values in the INS log")


def build_fnv_dataframe(df):
    """Restructure INS columns into MB-System's .fnv field layout."""
    timestamps = pd.to_datetime(df["date"] + " " + df["time"], format="%Y%m%d %H%M%S.%f")

    return pd.DataFrame({
        "year":      timestamps.dt.year,
        "month":     timestamps.dt.month,
        "day":       timestamps.dt.day,
        "hour":      timestamps.dt.hour,
        "minute":    timestamps.dt.minute,
        "second":    timestamps.dt.second + timestamps.dt.microsecond / 1e6,
        "longitude": df["longitude"],
        "latitude":  df["latitude"],
        "heading":   df["heading"],
    })


def main():
    args = parse_args()

    df = load_ins_log(args.input, verbose=args.verbose)
    validate_ins_log(df)
    fnv_df = build_fnv_dataframe(df)

    fnv_df.to_csv(args.output, sep=" ", header=False, index=False, float_format="%.6f")

    print(f"Wrote {len(fnv_df)} navigation fixes to {args.output}")


if __name__ == "__main__":
    main()
```

Every technique in this script — `argparse` setup, `pandas.read_csv` with a custom separator and column names, boolean/validation checks with `raise ValueError`, vectorized datetime handling with `.dt` accessors, and `to_csv` with explicit formatting — has been individually introduced and explained in Parts 8 through 12. Reading this script top to bottom should feel entirely comprehensible now, which is exactly the intended outcome: you are equipped to open the real `mbm_phins2fnv` source in `src/macros/` and recognize the same overall shape and the same `pandas` techniques, even though the real macro handles more INS log format variations and edge cases than this deliberately minimal reconstruction does.

### 13.3 Why the Validation Step Matters Here Specifically

Section 13.2's `validate_ins_log` function deserves particular attention, since it directly reflects a lesson from your own domain expertise: navigation and INS data is exactly the kind of data where a single corrupted or out-of-range record — a dropout, a sensor glitch, a parsing misalignment — can silently propagate into an `.fnv` file that all downstream MB-System processing then trusts implicitly. Checking `df.isnull().any().any()` (whether *any* column has *any* missing/unparseable value, `pandas`'s automatic representation for a field it could not convert to the expected type) and checking latitude/longitude against their physically valid ranges, *before* writing any output at all, is a direct, practical application of the evidence-driven, validate-before-trusting discipline that both companion primers have emphasized throughout.

---

## Part 14 — Writing Your Own Python-Based Macro

### 14.1 A Practical Checklist

1. Define the command-line interface first with `argparse`, including a clear `description` and `help` text for every option — this documents your macro's contract before you write a single line of transformation logic.
2. Write a dedicated loading function that reads the input with `pandas` (or plain Python file I/O, for simpler formats) and returns a well-structured DataFrame or list of records.
3. Write a dedicated validation function, checked immediately after loading, that raises a clear exception the moment any assumption about the data turns out to be false.
4. Write the actual transformation logic as one or more small, individually testable functions, each doing one clear job (restructuring columns, computing derived values, converting units).
5. Write the output using the appropriate `pandas`/standard-library writer, with explicit formatting.
6. Wrap everything in a `main()` function, guarded by `if __name__ == "__main__":`, so the script's individual functions remain safely importable and testable elsewhere.

### 14.2 A Worked Extension Exercise

Extend Section 13.2's script to also report, at the end, the total track length and time span covered by the converted navigation fixes — a natural, self-contained addition exercising several techniques already covered:

```python
import numpy as np

def summarize_track(fnv_df):
    lat_rad = np.radians(fnv_df["latitude"].to_numpy())
    lon_rad = np.radians(fnv_df["longitude"].to_numpy())

    earth_radius_m = 6371000.0
    dlat = np.diff(lat_rad)
    dlon = np.diff(lon_rad)

    approx_dist_m = earth_radius_m * np.sqrt(dlat**2 + (dlon * np.cos(lat_rad[:-1]))**2)
    total_distance_m = approx_dist_m.sum()

    print(f"Approximate track length: {total_distance_m / 1000.0:.2f} km")
```

`np.diff` computes the element-by-element difference between consecutive array entries — exactly "how far did latitude/longitude change between each pair of successive fixes" — and the small flat-Earth approximation above (adequate for a short survey track, not for long-range navigation) demonstrates, once more, how naturally `numpy`'s vectorized operations express this kind of per-step navigation calculation without an explicit loop.

---

## Part 15 — Debugging and Common Pitfalls

### 15.1 Reading Tracebacks

An unhandled Python exception prints a **traceback**: the exact chain of function calls leading to the error, ending with the specific line and the exception type/message. Always read a traceback from the bottom up — the last line tells you *what* went wrong; the lines above it tell you *where*, tracing back through every function call involved.

### 15.2 `pandas`-Specific Pitfalls

**Chained assignment warnings.** Modifying a DataFrame through a filtered/sliced view of it (`df[df["heading"] > 360]["heading"] = 0`) frequently produces a `SettingWithCopyWarning` and may silently fail to actually modify the original data, because the filtered view may be a *copy*, not the original DataFrame itself. Prefer `.loc[]` for any assignment that modifies specific rows/columns in place: `df.loc[df["heading"] > 360, "heading"] = 0`.

**Silent type coercion.** If a numeric column in a source file contains even one non-numeric value (a stray text note, a malformed field), `pandas` may read the entire column as generic `object` type rather than `float64`, silently disabling the numeric operations you expect to work — always check `df.dtypes` immediately after loading any new file, exactly as Section 10.1 recommends, rather than assuming a column loaded as the type you intended.

### 15.3 Environment Pitfalls

Forgetting to activate your virtual environment (Section 2.2) before running a script that depends on `pandas`/`numpy` produces a confusing `ModuleNotFoundError`, even though you know you already ran `pip install pandas numpy` — always confirm `which python3` points inside your intended virtual environment before debugging any deeper.

### 15.4 A Quick Syntax/Style Check

```sh
python3 -m py_compile myscript.py
```

This checks for syntax errors without running the script, directly analogous to Perl's `perl -c` from the companion primer.

---

## Part 16 — Where to Go Next

You now have a working foundation in Python as it applies to MB-System's newer, data-transformation-oriented macros: core syntax and data structures, functions and modules, string and regex processing, file I/O, and — most importantly for this specific corner of MB-System — a solid, hands-on introduction to `pandas` and `numpy` for reading, validating, transforming, and writing exactly the kind of tabular navigation and log data these newer macros exist to handle, plus `argparse` for a clean, self-documenting command-line interface.

The natural next step is to open `src/macros/mbm_phins2fnv` directly and read it start to finish, expecting to recognize nearly every technique — `pandas` column selection and transformation, datetime handling, `argparse` setup — from this primer, with any remaining unfamiliarity limited to the specific INS log format's exact column layout rather than the Python or `pandas` mechanics themselves. From there, a well-scoped first contribution might be adding support for an additional INS/navigation log format variant to an existing macro, or writing an entirely new small Python macro for a comparable conversion task, following the checklist in Part 14 and validating your output, as always, by inspecting it directly before trusting it against real survey data.
