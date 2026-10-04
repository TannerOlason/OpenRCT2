#!/usr/bin/env bash
# Fails if a fork source file is missing from the MSBuild project that Windows CI builds.
cd "$(dirname "$0")/../.." || exit 1
status=0
check() { # project, prefix to strip, files...
    local project=$1 prefix=$2; shift 2
    for f in "$@"; do
        local rel=${f#"$prefix"}
        rel=${rel//\//\\}
        grep -qF "Include=\"$rel\"" "$project" || { echo "missing from $project: $rel"; status=1; }
    done
}
check src/openrct2/libopenrct2.vcxproj src/openrct2/ src/openrct2/factory/*.{cpp,h} src/openrct2/factory/actions/*.{cpp,h}
check src/openrct2-ui/libopenrct2ui.vcxproj src/openrct2-ui/ src/openrct2-ui/windows/factory/*.cpp
check test/tests/tests.vcxproj test/tests/ test/tests/Factory*.cpp
exit $status
