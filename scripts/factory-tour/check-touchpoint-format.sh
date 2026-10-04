#!/usr/bin/env bash
# Runs clang-format over upstream files that carry FACTORY-TOUR touch points and reports only the hunks that touch
# fork lines. (The local clang-format may differ from CI's on untouched upstream code; those hunks are ignored.)
cd "$(dirname "$0")/../.." || exit 1
status=0
for f in $(git grep -l "FACTORY-TOUR" -- 'src/*.cpp' 'src/*.h' 'src/*.hpp' | grep -v -e "/factory/" -e "windows/factory/"); do
    hunks=$(clang-format --assume-filename="$f" < "$f" | diff -U2 "$f" - | awk '
        /^@@/ { if (hunk ~ /FACTORY-TOUR/) printf "%s", hunk; hunk = $0 "\n"; next }
        { if (hunk != "") hunk = hunk $0 "\n" }
        END { if (hunk ~ /FACTORY-TOUR/) printf "%s", hunk }')
    if [ -n "$hunks" ]; then
        echo "== $f"
        echo "$hunks"
        status=1
    fi
done
exit $status
