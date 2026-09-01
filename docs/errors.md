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

Use `--suggest-fix` to add an extra concrete fix line to diagnostics that already
have source-aware suggestions.

Current codes:

- `E_APPEND_TARGET`: a list helper expected a declared language list.
- `E_ASSIGN_TYPE`: an assignment or helper value does not match the expected type.
- `E_CONST_ASSIGN`: code tried to reassign a constant.
- `E_RANGE_ARGS`: `range(...)` received the wrong number of arguments.
- `E_IMPORT`: an import line is empty or malformed.
- `E_HELPER_ARGS`: a native helper received malformed arguments.
- `E_BLOCK_EXPECTED`: a block-opening line has no indented body.
- `E_INDENTATION`: a line is indented without a block opener before it.
- `E_RETURN_TYPE`: a return statement does not match the function signature.
- `E_PARAM_TYPE`: a function parameter does not use a simple typed form.

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

Unexpected indentation:

```text
int main()
    value = 1
        bad = 2
```

The fix is to remove the extra indentation or add a block-opening line before
the indented statement.

Invalid return type:

```text
int value()
    return "bad"
```

The fix is to return an `int`, or change the function signature to `char*`.
