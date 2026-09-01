# Language Reference

## Files

`.l` files are translated to `.c` files. `.H` files are translated to guarded
`.h` files. Regular `.c` and `.h` files can be passed through to GCC.

Generated `.c` files include `runtime.h` automatically.

## Blocks

Blocks are controlled by indentation. A line ending with `:` is accepted for
`if`, `elif`, `else`, `while`, `for ... in range(...)`, and `for item in list`.

## Variables

New assignments declare variables:

```text
count = 0
ratio = 2.5
ready = True
name = "Ada"
```

Existing assignments are type-checked when possible.

Constants cannot be reassigned:

```text
const limit = 10
```

## Conditions

`and`, `or`, and `not` become `&&`, `||`, and `!` outside strings.

```text
if score > 10 and not blocked:
    print("accepted")
elif score == 0:
    print("empty")
else:
    print("pending")
```

## Loops

```text
for i in range(5):
    print(i)

for i in range(2, 6):
    print(i)

for i in range(10, 0, -2):
    print(i)
```

Lists can be iterated directly:

```text
for value in values:
    print(value)
```

`break` and `continue` are emitted as C statements.

## Lists

Lists are homogeneous:

```text
numbers = [1, 2, 3]
words = ["one", "two"]
flags = [true, false]
```

Each list gets a generated length variable named `<list>_len`. Prefer `len(list)`
in source code.

Helpers:

- `append(list, value)`
- `insert(list, index, value)`
- `pop(list)`
- `contains(list, value)`

## Strings

String helpers allocate through `runtime.h` and are checked after allocation:

- `input()`
- `input("Prompt: ")`
- `str_copy(text)`
- `str_concat(left, right)`
- `str_len(text)`
- `str_eq(left, right)`

## Imports

```text
import tools
import "tools.H"
import <stdio.h>
```

Local `.H` imports are translated to `.h` includes. Duplicate includes are
emitted once in generated `.c` files.

## Generated C

Use `--emit-c` to keep generated C without building a binary. Use `-comments`
to include original source lines, and `--pretty-c` for extra spacing around
top-level functions.
