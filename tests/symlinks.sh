#!/usr/bin/env bash
set -euo pipefail

scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/bin" "$scratch/remote" "$scratch/remote/directory"
printf 'hello' > "$scratch/remote/regular"
printf 'newline' > "$scratch/remote/"$'target\n\n'
ln -s regular "$scratch/remote/file-link"
ln -s directory "$scratch/remote/directory-link"
ln -s absent "$scratch/remote/dangling-link"
ln -s "$scratch/remote/regular" "$scratch/remote/absolute-link"
ln -s $'target\n\n' "$scratch/remote/"$'odd\nlink'
ln -s cycle-link "$scratch/remote/cycle-link"

# Execute the worker's actual shell commands against local fixtures.
cat > "$scratch/bin/adb" <<'ADB'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 5 && $1 == -s && $2 == mock && $3 == shell && $4 == -T ]]
exec sh -c "$5"
ADB
chmod +x "$scratch/bin/adb"
export PATH="$scratch/bin:$PATH"
export QT_PLUGIN_PATH="$2${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export KIO_ADB_TEST_SERIAL=mock
"$1" symlinks unused "$scratch/remote" unused
