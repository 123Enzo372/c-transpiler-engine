#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

mkdir -p local_tests .tmp

gcc -Wall -Wextra main.c parser.c trad_c.c trad_h.c execute.c clean.c -o compilateur
COMPILER="./compilateur"
if [ -x "./compilateur.exe" ]; then
    COMPILER="./compilateur.exe"
fi

cat > local_tests/language_features.l <<'EOF'
int square(int x)
    return x * x

int main()
    const limit = 5
    values = [1, 2, 3]
    append(values, 4)
    insert(values, 1, 9)
    pop(values)
    total = 0
    for value in values:
        total = total + value
    found = contains(values, 9)
    copied = str_copy("ok")
    label = str_concat(copied, "!")
    if found and str_eq(copied, "ok"):
        print("total={total}; label={label}; len={str_len(label)}; square={square(limit)}")
    return 0
EOF

"$COMPILER" local_tests/language_features.l --emit-c --quiet
gcc -Wall -Wextra -I. local_tests/language_features.c -o local_tests/language_features
feature_output="$(local_tests/language_features)"
test "$feature_output" = "total=15; label=ok!; len=3; square=25"

cat > local_tests/input_feature.l <<'EOF'
int main()
    name = input("Name: ")
    greeting = str_concat("Hello ", name)
    print(greeting)
    return 0
EOF

"$COMPILER" local_tests/input_feature.l --emit-c --quiet
gcc -Wall -Wextra -I. local_tests/input_feature.c -o local_tests/input_feature
input_output="$(printf 'Ada\n' | local_tests/input_feature)"
test "$input_output" = "Name: Hello Ada"

cat > local_tests/type_error.l <<'EOF'
int main()
    values = [1, 2, 3]
    append(values, "bad")
    return 0
EOF

if "$COMPILER" local_tests/type_error.l --emit-c --quiet 2> local_tests/type_error.err; then
    echo "expected type_error.l to fail" >&2
    exit 1
fi
grep -q "E_ASSIGN_TYPE" local_tests/type_error.err

cat > local_tests/calc.H <<'EOF'
int add_one(int value);
EOF

cat > local_tests/calc.l <<'EOF'
int add_one(int value)
    return value + 1
EOF

cat > local_tests/import_main.l <<'EOF'
import calc

int main()
    print("import result={add_one(4)}")
    return 0
EOF

"$COMPILER" local_tests/import_main.l -o local_tests/import_app --quiet
import_output="$(local_tests/import_app)"
test "$import_output" = "import result=5"

cat > local_tests/run_feature.l <<'EOF'
int main()
    print("run command works")
    return 0
EOF

run_output="$("$COMPILER" run local_tests/run_feature.l --quiet)"
test "$run_output" = "run command works"

cat > local_tests/return_error.l <<'EOF'
int value()
    return "bad"
EOF

if "$COMPILER" local_tests/return_error.l --emit-c --quiet 2> local_tests/return_error.err; then
    echo "expected return_error.l to fail" >&2
    exit 1
fi
grep -q "E_ASSIGN_TYPE" local_tests/return_error.err

cat > local_tests/indent_error.l <<'EOF'
int main()
    value = 1
        bad = 2
    return 0
EOF

if "$COMPILER" local_tests/indent_error.l --emit-c --quiet 2> local_tests/indent_error.err; then
    echo "expected indent_error.l to fail" >&2
    exit 1
fi
grep -q "E_INDENTATION" local_tests/indent_error.err

"$COMPILER" deps local_tests/import_main.l > local_tests/deps.out
grep -q "local_tests/calc.H" local_tests/deps.out
grep -q "local_tests/calc.l" local_tests/deps.out

"$COMPILER" local_tests/language_features.l --teach --quiet
test -f local_tests/language_features.c
test -f local_tests/language_features.c.explain.txt
grep -q "Generated C explanation" local_tests/language_features.c.explain.txt
grep -q "Source:" local_tests/language_features.c.explain.txt

if "$COMPILER" local_tests/type_error.l --emit-c --quiet --suggest-fix 2> local_tests/suggest_fix.err; then
    echo "expected suggest-fix type_error.l to fail" >&2
    exit 1
fi
grep -q "Suggested fix" local_tests/suggest_fix.err

printf 'print("repl ok")\n:run\n:quit\n' | "$COMPILER" repl --quiet > local_tests/repl.out
grep -q "repl ok" local_tests/repl.out

echo "All local tests passed."
