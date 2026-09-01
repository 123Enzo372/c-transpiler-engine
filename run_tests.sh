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

echo "All local tests passed."
