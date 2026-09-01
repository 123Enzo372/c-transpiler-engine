# Custom Language Transpiler

This project is a small source-to-source transpiler written in C. It reads a
compact educational language, translates `.l` files into C source files, translates
`.H` files into guarded generated C headers, and can call `gcc` to build the final executable.

The language keeps C close at hand, but adds a few conveniences:

- indentation-based blocks, similar to Python;
- `elif` branches for multi-way conditionals;
- simple type inference for assignments;
- `and`, `or`, and `not` as readable logical operators;
- `print(...)` with string interpolation;
- homogeneous list literals;
- `len(list)` and `append(list, value)` helpers;
- `range(stop)`, `range(start, stop)`, and `range(start, stop, step)` loops;
- `break` and `continue`;
- `input(...)` for reading a line from standard input;
- `str_copy(text)` and `str_concat(left, right)` for dynamically allocated strings;
- `str_len(text)`, `str_eq(left, right)`, and `contains(list, value)`;
- `insert(list, index, value)`, `pop(list)`, and `for item in list` iteration;
- `const name = value` bindings;
- ergonomic `import ...` lines for local `.H` headers and system headers;
- list decomposition with `head::tail`;
- `match ... with` branches;
- helper directives for common includes, POSIX processes, and pipes.

## Requirements

The compiler frontend invokes `gcc` through POSIX APIs such as `fork`, `execvp`,
and `waitpid`. It is therefore intended for Linux, macOS, or WSL on Windows.
On native Windows, use the MSYS2 MSYS shell and the MSYS `gcc` package. The
UCRT64/MinGW GCC package is useful for native Windows programs, but it does not
provide the POSIX headers used by this project.

You need:

- `gcc`
- a standard C library
- a POSIX-like shell/runtime to execute the produced binary

## Build The Transpiler

From the project root:

```bash
gcc main.c parser.c trad_c.c trad_h.c execute.c clean.c -o compilateur
```

This creates an executable named `compilateur`.
On MSYS2/Windows, GCC may create `compilateur.exe`; run that executable from the
MSYS shell.

## Quick Start

Create `hello.l`:

```text
int main()
    name = "Ada"
    print("Hello {name}")
    return 0
```

Translate and compile it:

```bash
./compilateur hello.l -o hello
./hello
```

Expected output:

```text
Hello Ada
```

## What The Tool Does

| Input | Output | Purpose |
| --- | --- | --- |
| `.l` | `.c` | Translates the custom language into C. |
| `.H` | `.generated.h` | Creates a C header with an automatic include guard. |
| `.c` / `.h` | passed to `gcc` | Standard C files can be compiled alongside generated files. |

By default, generated `.c` and `.h` files are temporary: they are removed after a
successful build. Use `-keep_c` or `-keep_h` when you want to inspect them.

Compiled binaries and common temporary build artifacts are ignored by Git through
`.gitignore`.

## Command Line

```bash
./compilateur [sources] [gcc-options] [tool-options] -o output_name
```

Examples:

```bash
./compilateur main.l -o app
./compilateur run main.l
./compilateur init my-app
./compilateur clean
./compilateur repl
./compilateur deps main.l
./compilateur main.l --teach
./compilateur main.l --explain-generated --suggest-fix
./compilateur main.l utils.l api.H -Wall -Wextra -o app
./compilateur main.l -without-binary -keep_c
./compilateur main.l --emit-c --pretty-c
./compilateur main.l -without-binary -keep_c -comments
./compilateur api.H -without-binary -keep_h
./compilateur --version
./compilateur --explain E_ASSIGN_TYPE
./compilateur --help
```

## Options

| Option | Description |
| --- | --- |
| `-o <name>` | Sets the final executable name passed to `gcc`. |
| `run <source.l>` | Translates, compiles, then runs the produced binary. |
| `init [name]` | Creates a small starter project without overwriting existing files. |
| `clean`, `--clean` | Removes common build artifacts such as local binaries and `.gch` files. |
| `repl` | Opens a small teaching REPL. Use `:run`, `:show`, `:reset`, and `:quit`. |
| `deps <source>` | Prints the local import graph resolved by the CLI. |
| `-without-binary` | Translates sources but does not run `gcc`. |
| `--emit-c`, `-S` | Translates sources, keeps generated `.c`/`.h` files, and does not run `gcc`. |
| `-keep_c` | Keeps generated `.c` files after the build. |
| `-keep_h` | Keeps generated `.h` files after the build. |
| `-comments` | Adds source-line comments to generated `.c` files. Useful with `-keep_c`. |
| `--pretty-c` | Adds extra spacing around top-level functions in generated C. |
| `--trace`, `--dump-ast` | Prints recognized translation steps while translating. |
| `--quiet` | Hides success messages while keeping errors visible. |
| `--no-color` | Disables ANSI colors in diagnostics. Colors are only used on interactive terminals. |
| `--teach` | Enables readable generated C, source comments, concrete suggestions, and `.explain.txt` output. |
| `--explain-generated` | Writes a `.c.explain.txt` file that maps source lines to generated C. |
| `--suggest-fix` | Adds an extra concrete fix line to diagnostics that already have suggestions. |
| `--version` | Prints the transpiler version and exits. |
| `--explain <code>` | Prints a short explanation and suggestion for an error code. |
| `-rm_l` | Deletes original `.l` files after a successful run. |
| `-rm_H` | Deletes original `.H` files after a successful run. |
| `-french` | Prints transpiler diagnostics in French. |
| `-h`, `--help` | Prints command-line help and exits. |

Unknown options that start with `-`, plus regular `.c` and `.h` files, are passed
through to `gcc`. This lets you use flags such as `-Wall`, `-O2`, `-g`, or `-lm`.

## Syntax Rules

The parser reads source files line by line, so spaces inside strings are
preserved. Indentation still matters.

- Write one statement per line.
- Use consistent indentation, preferably 4 spaces.
- Do not write C braces for `.l` blocks.
- A block starts when the next line is more indented.
- A block ends when indentation decreases.
- Keep spaces around keywords such as `if`, `while`, `for`, and `match`.
- Raw C lines are allowed, but complex C should keep normal C syntax.
- A final newline is recommended, but the last source line is still processed if
  the file does not end with one.

## `.l` Language Guide

### Functions

Function declarations look like C. The function body is controlled by indentation.

```text
int square(int x)
    return x * x

int main()
    result = square(6)
    print("result={result}")
    return 0
```

### Variables And Type Inference

Assigning to a new name declares a variable. The type is inferred from the value.

```text
age = 42
temperature = 19.5
ratio = temperature / 2.0
passed = age > 18
initial = 'A'
message = "ready"
active = True
const max_score = 20
```

`True` and `False` are normalized to C's `true` and `false`.
Expressions containing decimal or exponent literals, or variables typed as
`float`/`double`, keep a floating-point type. Comparisons and logical expressions
infer `bool`.
Assignments to existing variables are type-checked when the transpiler can infer
both sides. For example, assigning a string to an `int` variable reports
`E_ASSIGN_TYPE` before GCC is invoked.

Assigning to an existing variable becomes a normal C assignment:

```text
score = 0
score = score + 10
```

Constants use the same inference but cannot be reassigned:

```text
const limit = 10
```

### Conditions

```text
if score >= 10:
    print("passed")
elif score >= 5:
    print("almost")
else:
    print("try again")
```

The `:` is accepted for readability after `if`, `elif`, `while`, and `else`.
Indentation still defines the block.

Logical words are translated to C operators outside strings:

```text
if score >= 10 and not blocked:
    print("allowed")
elif score == 0 or score == 1:
    print("small")
```

### Loops

`while` loop:

```text
i = 0
while i < 3:
    print("i={i}")
    i = i + 1
```

`for ... in range(...)` loop:

```text
for i in range(5):
    print("turn {i}")

for i in range(2, 6):
    print("from two to five: {i}")

for i in range(10, 0, -2):
    if i == 4:
        continue
    if i == 2:
        break
    print("descending {i}")
```

The first form starts at `0`, the second starts at `start`, and the third uses a
custom `step`. A negative step counts downward.

`range(5)` is translated conceptually as:

```c
for (int i = 0; i < 5; i++)
```

### Printing

`print(...)` becomes `printf(...)` with a newline.

```text
name = "Ada"
age = 36
numbers = [10, 20, 30]
print("name={name}, age={age}")
print(age)
print("first number={numbers[0]}")
```

String interpolation uses `{variable}` inside double-quoted strings.
Interpolations must be closed and cannot be empty.

### Lists

A list literal creates a dynamically allocated C array and a `<name>_len` variable.
Use `len(name)` in source code when you want the length.

```text
numbers = [1, 2, 3, 4]
print("length={len(numbers)}")
print("first={numbers[0]}")

append(numbers, 5)
print("new length={len(numbers)}")
print("last={numbers[len(numbers) - 1]}")
insert(numbers, 1, 99)
pop(numbers)
print("has 99? {contains(numbers, 99)}")
```

Supported homogeneous list examples:

```text
integers = [1, 2, 3]
floats = [1.5, 2.5, 3.5]
words = ["one", "two", "three"]
flags = [true, false, true]
```

Lists must be homogeneous. Mixed lists such as `[1, 2.5, "text"]` are rejected.
Generated arrays are freed automatically before `return` and when their block
scope closes.

`append(list, value)` reallocates the generated array, writes the new value, and
increments the generated length. It expects a list created by this language.
`insert(list, index, value)` inserts before `index`, `pop(list)` removes the last
item when the list is not empty, and `contains(list, value)` returns a boolean.
List mutation helpers validate the element type before generating C.

Iterate over a list with `for item in list:`:

```text
for value in numbers:
    print(value)
```

### Input And Strings

`input()` reads one line from standard input, removes the trailing newline, and
returns a dynamically allocated string. `input("prompt")` prints the prompt first.

```text
int main()
    name = input("Your name: ")
    greeting = str_concat("Hello ", name)
    print(greeting)
    return 0
```

`str_copy(text)` allocates a mutable copy of a string. `str_concat(left, right)`
allocates a new string, copies `left`, then appends `right`. `str_len(text)`
returns the string length as an `int`, and `str_eq(left, right)` compares strings.
Reassigning a string created by `input`, `str_copy`, or `str_concat` frees the
old allocation only after the replacement allocation succeeds.

### List Decomposition

```text
numbers = [10, 20, 30]
head::tail = numbers
print("head={head}, tail length={tail_len}")
```

This creates:

- `head`, the first element;
- `tail`, a pointer to the next element;
- `tail_len`, the remaining length.

### Match

`match ... with` becomes an `if` / `else if` / `else` chain.

```text
n = 2
match n with
    | 0 -> print("zero")
    | 1 -> print("one")
    | _ if n > 10 -> print("large")
    | _ -> print("other")
```

List patterns are also supported:

```text
items = [10, 20, 30]
match items with
    | [] -> print("empty")
    | [x] -> print("one item: {x}")
    | head::tail -> print("many items, first={head}")
```

Match branch actions must fit on one line.

## Directives In `.l` Files

Local and system imports can be written without raw `#include` syntax:

```text
import math_tools
import "other_tools.H"
import <stdio.h>
```

These become:

```c
#include "math_tools.generated.h"
#include "other_tools.generated.h"
#include <stdio.h>
```

The CLI also scans local language imports before translation. If `main.l`
contains `import tools`, existing `tools.H` and `tools.l` files beside `main.l`
are added automatically to the translation list. `import tools` and
`import "tools.H"` include `tools.generated.h`, which avoids `.H`/`.h` filename
collisions on case-insensitive file systems. `import "tools.h"` still includes a
regular C header. System imports such as `import <stdio.h>` stay as normal C
includes.

| Directive | Generated C |
| --- | --- |
| `#all` | Adds `math.h` and `time.h`. Basic includes are already emitted for `.c` files. |
| `#linux` | Adds `unistd.h`, `sys/types.h`, and `sys/wait.h`. |
| `#pipe` | Declares `int pipe_fd[2]` and calls `pipe(pipe_fd)`. |
| `#process` | Calls `fork()` and declares `pid_t pid`. |
| `#enfant` | Emits `if (pid == 0)`. |
| `#parent` | Emits `else`. |
| `#parent basic` | Emits `else`, waits for the child with `waitpid`, and checks its exit status. |

Unknown `#` lines in `.l` files become C comments.
Raw `// ...` and `/* ... */` comment lines are preserved as C comments.

## Diagnostics And Safety Checks

Diagnostics follow the same language-selection logic across the codebase:
English by default, French when `-french` is present.

When the transpiler can point to a source line, errors include the file and line,
an excerpt, a caret, and a practical suggestion:

```text
ERROR [Line 3] : append(...) expects an already declared list.
  --> demo.l:3
   |
   3 | append(values, 4)
   |        ^
Suggestion: Create the list first with values = [] or values = [1, 2, 3].
```

The transpiler now reports clear errors for oversized source lines, oversized
assignments, oversized list literals, too many list elements, malformed list
decomposition, malformed `match` branches, malformed `print` interpolation,
malformed helper arguments, invalid `range(...)` syntax, invalid `len(...)` or
`append(...)` targets, malformed `import ...` lines, unexpected indentation,
missing block bodies, malformed function parameters, and incompatible return
values.
The parser preserves each source line before translation, which avoids losing
spacing inside strings and keeps the final line even without a trailing newline.

Generated C checks dynamic allocations from list literals, `append(...)`,
`insert(...)`, `input(...)`, `str_copy(...)`, and `str_concat(...)`. Before a
generated early `return`, allocated pointers are freed once per name and then set
to `NULL`. Runtime helpers live in `runtime.h`, which is included automatically
by generated `.c` files. Function parameters from simple C-style signatures are
visible inside the function body, and `return` statements are checked against the
declared return type when the transpiler can infer the returned expression.

Error codes can be explained from the CLI:

```bash
./compilateur --explain E_ASSIGN_TYPE
```

## `.H` Header Files

`.H` files are translated into `.generated.h` files and wrapped in an include
guard generated from the file name.

Input `maths.H`:

```text
#all
import vectors
import <stddef.h>

int square(int x);
```

Output `maths.generated.h`:

```c
#ifndef MATHS_H
#define MATHS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "vectors.generated.h"
#include <stddef.h>

int square(int x);

#endif /* MATHS_H */
```

Recognized `.H` directives:

| Directive | Includes |
| --- | --- |
| `#all` | `stdio.h`, `stdlib.h`, `string.h`, `stdbool.h`, `math.h`, `time.h` |
| `#linux` | `unistd.h`, `sys/types.h`, `sys/wait.h`, `fcntl.h` |
| `#windows` | `windows.h` |
| `#mac` | `TargetConditionals.h`, `Availability.h` |

`.H` files also support the same ergonomic `import ...` syntax as `.l` files:
`import tools` becomes `#include "tools.generated.h"`,
`import "other_tools.H"` becomes `#include "other_tools.generated.h"`,
`import "tools.h"` keeps `#include "tools.h"`, and `import <stddef.h>` stays a
system include.

## Complete Example

`demo.l`:

```text
#all
#linux

int sum_until(int n)
    total = 0
    for i in range(n):
        total = total + i
    return total

int main()
    limit = 5
    total = sum_until(limit)
    print("sum before {limit} = {total}")

    values = [10, 20, 30]
    append(values, 40)
    print("values length={len(values)}")
    head::tail = values
    print("first={head}, remaining={tail_len}")

    name = input("Name: ")
    greeting = str_concat("Hello ", name)
    print(greeting)

    match total with
        | 0 -> print("zero")
        | _ if total > 5 and len(values) > 3 -> print("greater than five")
        | _ -> print("small total")

    return 0
```

Build it:

```bash
./compilateur demo.l -o demo
```

Inspect generated C instead of compiling:

```bash
./compilateur demo.l -without-binary -keep_c
```

Inspect generated C with source-line comments:

```bash
./compilateur demo.l -without-binary -keep_c -comments
```

Teaching mode keeps readable C and writes a sidecar explanation:

```bash
./compilateur demo.l --teach
```

Run the local verification suite:

```bash
bash run_tests.sh
```

The test script creates temporary fixtures under `local_tests/`, which is ignored
by Git.

More detailed references are available in:

- `docs/tutorial.md`
- `docs/language_reference.md`
- `docs/errors.md`

Basic VS Code language support is available in `vscode-language/`. Open that
folder in VS Code and press `F5` to test syntax highlighting and snippets in an
Extension Development Host.


## Known Limits

This project is a lightweight transpiler, not a full C compiler.

- The parser does not understand the full C grammar.
- Type inference is limited.
- `match` branch actions are single-line only.
- Lists are C arrays allocated with `malloc`.
- `input(...)` currently reads at most 255 characters plus the terminating `\0`.
- Mixed-type lists are rejected.
- Process and pipe helpers depend on POSIX APIs.
- For complex C, write the C line explicitly; typed C declarations and common
  C calls can be mixed with `.l` syntax.

To understand what the language produces, read `example_syntax` and run examples
with `-without-binary -keep_c`.
