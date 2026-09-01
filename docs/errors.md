# Error Guide

The transpiler reports language-level errors before GCC whenever it can. Most
diagnostics include:

- an error message;
- a file and line number;
- the original source excerpt;
- a caret pointing near the problem;
- a practical suggestion.

## Error Codes

Use `--explain <code>` to get a short explanation from the CLI.

```bash
./compilateur --explain E_ASSIGN_TYPE
```

Current codes:

- `E_APPEND_TARGET`: a list helper expected a declared language list.
- `E_ASSIGN_TYPE`: an assignment or helper value does not match the expected type.
- `E_CONST_ASSIGN`: code tried to reassign a constant.
- `E_RANGE_ARGS`: `range(...)` received the wrong number of arguments.
- `E_IMPORT`: an import line is empty or malformed.
- `E_HELPER_ARGS`: a native helper received malformed arguments.

## Examples

Invalid list mutation:

```text
int main()
    name = "Ada"
    append(name, "!")
```

The fix is to create a list first:

```text
int main()
    names = ["Ada"]
    append(names, "!")
```

Invalid type:

```text
int main()
    values = [1, 2, 3]
    append(values, "bad")
```

The fix is to append an `int`, or use a string list.

Invalid constant reassignment:

```text
int main()
    const limit = 10
    limit = 20
```

The fix is to create a new variable for the changed value.
