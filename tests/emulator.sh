#!/usr/bin/env bash
set -euo pipefail

: "${ANDROID_SDK:?Set ANDROID_SDK to the Android SDK directory}"
emulator="$ANDROID_SDK/emulator/emulator"
adb="$ANDROID_SDK/platform-tools/adb"
export PATH="$ANDROID_SDK/platform-tools:$PATH"
export QT_PLUGIN_PATH="$2${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

avd=${KIO_ADB_TEST_AVD:-}
if [[ -z $avd ]]; then
    available=$("$emulator" -list-avds)
    if [[ -z $available || $available == *$'\n'* ]]; then
        echo "Create a test AVD and set KIO_ADB_TEST_AVD to its name." >&2
        exit 1
    fi
    avd=$available
fi

port=${KIO_ADB_TEST_PORT:-5580}
if [[ ! $port =~ ^[0-9]{4}$ ]] || (( port < 5554 || port > 5682 || port % 2 )); then
    echo "KIO_ADB_TEST_PORT must be an even emulator console port from 5554 through 5682." >&2
    exit 1
fi
export ANDROID_SERIAL="emulator-$port"
devices=$("$adb" devices)
if [[ $devices == *"$ANDROID_SERIAL"$'\t'* ]]; then
    echo "$ANDROID_SERIAL is already registered with ADB." >&2
    exit 1
fi

scratch=$(mktemp -d)
log="$scratch/emulator.log"
probe="$scratch/readiness.txt"
printf '%s\n' 'KIO ADB storage readiness' > "$probe"
remote_probe="/sdcard/kio-adb-ready-${scratch##*.}.txt"
"$emulator" -avd "$avd" -port "$port" -wipe-data -no-snapshot -no-window -no-audio -no-boot-anim >"$log" 2>&1 &
pid=$!
cleanup() {
    status=$?
    trap - EXIT
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    # ADB may retain the serial briefly after the emulator process exits.
    for ((attempt = 0; attempt < 20; attempt++)); do
        devices=$(timeout 2 "$adb" devices) || break
        [[ $devices == *"$ANDROID_SERIAL"$'\t'* ]] || break
        sleep 0.25
    done
    if (( status != 0 )); then
        cat "$log" >&2
    fi
    rm -f "$log" "$probe" "$scratch/probe.log" "$scratch/fixture.txt" "$scratch/downloaded.txt"
    rmdir "$scratch"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

deadline=$((SECONDS + 300))
while (( SECONDS < deadline )); do
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "Emulator exited before Android finished booting." >&2
        exit 1
    fi
    if boot=$(timeout 5 "$adb" shell getprop sys.boot_completed 2>/dev/null) &&
        [[ ${boot//$'\r'/} == 1 ]] &&
        timeout 5 "$adb" push "$probe" "$remote_probe" >"$scratch/probe.log" 2>&1 &&
        timeout 5 "$adb" shell -T "cat '$remote_probe'" | cmp -s "$probe" - &&
        timeout 5 "$adb" shell -T "rm '$remote_probe'"; then
        action=$3
        fixture="$scratch/fixture.txt"
        name="kio-adb-$action-${scratch##*.}.txt"
        remote="/sdcard/$name"
        printf '%s\n' "KIO ADB $action fixture" > "$fixture"
        if [[ $action == download || $action == list ]]; then
            timeout 15 "$adb" push "$fixture" "$remote"
        fi
        timeout 60 "$1" "$action" "$name" "$fixture" "$scratch/downloaded.txt"
        case "$action" in
            download) cmp "$fixture" "$scratch/downloaded.txt" ;;
            upload)
                timeout 15 "$adb" shell -T "cat '$remote'" > "$scratch/downloaded.txt"
                cmp "$fixture" "$scratch/downloaded.txt"
                ;;
        esac
        exit
    fi
    sleep 2
done

if [[ -f $scratch/probe.log ]]; then
    cat "$scratch/probe.log" >&2
fi
echo "Timed out waiting for Android and shared storage on $ANDROID_SERIAL." >&2
exit 1
