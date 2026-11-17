#!/usr/bin/env bash
set -euo pipefail

scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/remote"
export MOCK_ADB_STORAGE="$scratch/remote"
export KIO_ADB_TEST_SERIAL=mock
export MOCK_ADB_ACTION="$3"
export PATH="$scratch:$PATH"
export QT_PLUGIN_PATH="$2${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

cat > "$scratch/adb" <<'ADB'
#!/usr/bin/env bash
set -euo pipefail
if [[ $MOCK_ADB_ACTION == list-devices ]]; then
    if [[ $1 == devices && $2 == -l ]]; then
        printf '%s\n' 'List of devices attached' \
            'mock device transport_id:1' \
            '192.0.2.12:5555 device transport_id:2' \
            'adb-AbC-XYZ._adb-tls-connect._tcp device transport_id:3' \
            'unavailable unauthorized transport_id:4' \
            'rescue recovery transport_id:5' \
            'disconnected offline transport_id:6'
        exit 0
    fi
    [[ $1 == -s && $3 == shell && $5 == "(stat -L"* ]] || exit 1
    case "$2" in
        mock|rescue|192.0.2.12:5555|adb-AbC-XYZ._adb-tls-connect._tcp) ;;
        *) exit 1 ;;
    esac
else
    [[ $1 == -s && $2 == "$KIO_ADB_TEST_SERIAL" ]] || exit 1
fi
shift 2
case "$1" in
    shell)
        case "$3" in
            "(stat -L"*"/sdcard'"*) printf '41ff 4096 1700000000\000\000' ;;
            "(stat -L"*"/sdcard/first.txt'"*|"(stat -L"*"/sdcard/second file.txt'"*)
                printf '81b4 10 1700000000\000\000'
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
        if [[ $MOCK_ADB_ACTION == upload-error ]]; then
            printf "adb: remote permission denied\n" >&2
            exit 1
        fi
        if [[ $MOCK_ADB_ACTION == upload-stalled ]]; then
            exec sleep 160
        fi
        if [[ $MOCK_ADB_ACTION == upload-active ]]; then
            # Like ADB, the mock only reports live activity on a smart terminal.
            for percent in 25 50; do
                sleep 16
                if [[ -t 2 && ${TERM:-dumb} != dumb ]]; then
                    printf '\r[%3d%%] %s\033[K' "$percent" "$3" >&2
                fi
            done
        fi
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
    list-devices) "$1" list-devices unused unused unused ;;
    upload-multiple)
        "$1" upload-multiple unused "$scratch/first.txt" "$scratch/second file.txt"
        cmp "$scratch/first.txt" "$MOCK_ADB_STORAGE/first.txt"
        cmp "$scratch/second file.txt" "$MOCK_ADB_STORAGE/second file.txt"
        ;;
    upload-active)
        "$1" upload first.txt "$scratch/first.txt" unused
        cmp "$scratch/first.txt" "$MOCK_ADB_STORAGE/first.txt"
        ;;
    upload-error)
        "$1" upload-error first.txt "$scratch/first.txt" unused
        ;;
    upload-stalled)
        "$1" upload-stalled first.txt "$scratch/first.txt" unused
        [[ ! -e "$MOCK_ADB_STORAGE/first.txt" ]]
        ;;
    delete-multiple)
        cp -- "$scratch/first.txt" "$scratch/second file.txt" "$MOCK_ADB_STORAGE/"
        "$1" delete-multiple first.txt 'second file.txt' unused
        [[ ! -e "$MOCK_ADB_STORAGE/first.txt" && ! -e "$MOCK_ADB_STORAGE/second file.txt" ]]
        ;;
    *) exit 1 ;;
esac
