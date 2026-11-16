#!/usr/bin/env bash
set -euo pipefail

scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/remote"
export MOCK_ADB_STORAGE="$scratch/remote"
export PATH="$scratch:$PATH"
export QT_PLUGIN_PATH="$2${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

cat > "$scratch/adb" <<'ADB'
#!/usr/bin/env bash
set -euo pipefail
if [[ $1 == -s ]]; then
    shift 2
fi
case "$1" in
    get-serialno) echo mock ;;
    shell)
        case "$3" in
            "stat -L"*"/sdcard'"*) echo '41ff 4096 1700000000' ;;
            "stat -L"*"/sdcard/first.txt'"*|"stat -L"*"/sdcard/second file.txt'"*)
                echo '81b4 10 1700000000'
                ;;
            "rm '/sdcard/first.txt'") rm -- "$MOCK_ADB_STORAGE/first.txt" ;;
            "rm '/sdcard/second file.txt'")
                # Let DeleteJob publish progress while the last deletion is pending.
                sleep 1
                rm -- "$MOCK_ADB_STORAGE/second file.txt"
                ;;
            "test -e "*) exit 1 ;;
            "if [ -L "*|"chmod "*|"touch "*) ;;
            *) exit 1 ;;
        esac
        ;;
    push)
        printf '[ 25%%] %s\r' "$3"
        # Give the CopyJob reporting timer time to publish partial progress.
        sleep 1
        printf '[ 75%%] %s\n' "$3"
        cp -- "$2" "$MOCK_ADB_STORAGE/${3##*/}"
        ;;
    *) exit 1 ;;
esac
ADB
chmod +x "$scratch/adb"

printf '%s\n' 'First upload fixture' > "$scratch/first.txt"
printf '%s\n' 'Second upload fixture with spaces in its name' > "$scratch/second file.txt"
case "$3" in
    upload-multiple)
        "$1" upload-multiple unused "$scratch/first.txt" "$scratch/second file.txt"
        cmp "$scratch/first.txt" "$MOCK_ADB_STORAGE/first.txt"
        cmp "$scratch/second file.txt" "$MOCK_ADB_STORAGE/second file.txt"
        ;;
    delete-multiple)
        cp -- "$scratch/first.txt" "$scratch/second file.txt" "$MOCK_ADB_STORAGE/"
        "$1" delete-multiple first.txt 'second file.txt' unused
        [[ ! -e "$MOCK_ADB_STORAGE/first.txt" && ! -e "$MOCK_ADB_STORAGE/second file.txt" ]]
        ;;
    *) exit 1 ;;
esac
