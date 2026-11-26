# shellcheck shell=bash
# The DLLs a Windows package needs, for deploy/windows/package.sh and
# deploy/board-tool/package.sh (sourced; run in an MSYS2 UCRT64 shell).

# Copies the DLLs that the program and its DLLs load from MSYS2 or the dependency prefix, until
# nothing new turns up. Windows' own DLLs stay out.
copy_libraries() { # folder dependencies
    local folder=$1 dependencies=$2 added=1 dll
    while [ "$added" -gt 0 ]; do
        added=0
        while IFS= read -r dll; do
            [ -e "$folder/$(basename "$dll")" ] && continue
            cp "$dll" "$folder/"
            added=$((added + 1))
        done < <(find "$folder" -name '*.exe' -o -name '*.dll' | while read -r file; do
            PATH="$dependencies/bin:$PATH" ldd "$file" | awk '{print $3}' |
                grep -iE '^(/ucrt64/bin|'"$dependencies"'/bin)/' || true
        done | sort -u)
    done
}

# Fails when a program or DLL in the package needs a DLL that is neither in the package nor part
# of Windows, which Windows reports in a dialog rather than an error code. Windows' own DLLs are
# not followed: some of what they name exists only on some editions.
check_libraries() {
    local ntldd missing
    ntldd=$(command -v ntldd)
    missing=$(cd "$1" && find . -name '*.exe' -o -name '*.dll' | while read -r file; do
        PATH="$PWD:$(cygpath -u "$SYSTEMROOT")/System32" "$ntldd" "$file" | grep -i 'not found' |
            grep -viE '(api|ext)-ms-' | sed "s|^|$file: |" || true # API sets are not files
    done)
    if [ -n "$missing" ]; then
        printf 'package.sh: the package needs DLLs that are not in it:\n%s\n' "$missing" >&2
        exit 1
    fi
}
