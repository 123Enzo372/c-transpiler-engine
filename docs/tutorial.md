# Tutorial

This tutorial walks through a small program that uses the most useful language
features.

## 1. Write A Program

Create `demo.l`:

```text
int main()
    const limit = 5
    values = [1, 2, 3]
    append(values, 4)
    insert(values, 1, 9)
    pop(values)

    total = 0
    for value in values:
        total = total + value

    label = str_copy("sum")
    message = str_concat(label, "=")

    if contains(values, 9) and str_eq(label, "sum"):
        print("{message}{total} below {limit}? {total < limit}")

    return 0
```

## 2. Translate And Inspect C

```bash
./compilateur demo.l --emit-c --pretty-c -comments
```

This keeps `demo.c` and adds comments that show the original source lines.

## 3. Build And Run

```bash
./compilateur demo.l -o demo
./demo
```

## 4. Debug A Translation

Use trace mode to see what the transpiler recognizes:

```bash
./compilateur demo.l --emit-c --trace
```

If an error has a code, ask for the explanation:

```bash
./compilateur --explain E_ASSIGN_TYPE
```
