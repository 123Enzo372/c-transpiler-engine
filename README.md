# Custom Language Transpiler

This project is a small source-to-source transpiler written in C. It reads a
compact educational language, translates `.l` files into C source files, translates
`.H` files into guarded C headers, and can call `gcc` to build the final executable.

The language keeps C close at hand, but adds a few conveniences:

- indentation-based blocks, similar to Python;
- simple type inference for assignments;
- `print(...)` with string interpolation;
- homogeneous list literals;
- list decomposition with `head::tail`;
- `match ... with` branches;
- helper directives for common includes, POSIX processes, and pipes.

## Requirements

The compiler frontend invokes `gcc` through POSIX APIs such as `fork`, `execvp`,
and `waitpid`. It is therefore intended for Linux, macOS, or WSL on Windows.

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
| `.H` | `.h` | Creates a C header with an automatic include guard. |
| `.c` / `.h` | passed to `gcc` | Standard C files can be compiled alongside generated files. |

By default, generated `.c` and `.h` files are temporary: they are removed after a
successful build. Use `-keep_c` or `-keep_h` when you want to inspect them.

## Command Line

```bash
./compilateur [sources] [gcc-options] [tool-options] -o output_name
```

Examples:

```bash
./compilateur main.l -o app
./compilateur main.l utils.l api.H -Wall -Wextra -o app
./compilateur main.l -without-binary -keep_c
./compilateur api.H -without-binary -keep_h
```

## Options

| Option | Description |
| --- | --- |
| `-o <name>` | Sets the final executable name passed to `gcc`. |
| `-without-binary` | Translates sources but does not run `gcc`. |
| `-keep_c` | Keeps generated `.c` files after the build. |
| `-keep_h` | Keeps generated `.h` files after the build. |
| `-rm_l` | Deletes original `.l` files after a successful run. |
| `-rm_H` | Deletes original `.H` files after a successful run. |
| `-french` | Prints transpiler diagnostics in French. |

Unknown options that start with `-`, plus regular `.c` and `.h` files, are passed
through to `gcc`. This lets you use flags such as `-Wall`, `-O2`, `-g`, or `-lm`.

## Syntax Rules

The parser is intentionally simple. Whitespace matters.

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
initial = 'A'
message = "ready"
active = True
```

`True` and `False` are normalized to C's `true` and `false`.

Assigning to an existing variable becomes a normal C assignment:

```text
score = 0
score = score + 10
```

### Conditions

```text
if score >= 10:
    print("passed")
else:
    print("try again")
```

The `:` is accepted for readability. Indentation still defines the block.

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
```

This is translated conceptually as:

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

```text
numbers = [1, 2, 3, 4]
print("length={numbers_len}")
print("first={numbers[0]}")
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

## Diagnostics And Safety Checks

Diagnostics follow the same language-selection logic across the codebase:
English by default, French when `-french` is present.

The transpiler now reports clear errors for oversized source lines, oversized
assignments, oversized list literals, too many list elements, malformed list
decomposition, malformed `match` branches, and malformed `print` interpolation.

## `.H` Header Files

`.H` files are translated into `.h` files and wrapped in an include guard generated
from the file name.

Input `maths.H`:

```text
#all

int square(int x);
```

Output `maths.h`:

```c
#ifndef MATHS_H
#define MATHS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

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
    head::tail = values
    print("first={head}, remaining={tail_len}")

    match total with
        | 0 -> print("zero")
        | _ if total > 5 -> print("greater than five")
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


## Known Limits

This project is a lightweight transpiler, not a full C compiler.

- The parser does not understand the full C grammar.
- Type inference is limited.
- `match` branch actions are single-line only.
- Lists are C arrays allocated with `malloc`.
- Mixed-type lists are rejected.
- Process and pipe helpers depend on POSIX APIs.
- For complex C, write the C line explicitly; typed C declarations and common
  C calls can be mixed with `.l` syntax.

To understand what the language produces, read `example_syntax` and run examples
with `-without-binary -keep_c`.
