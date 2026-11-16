# KIO ADB

Browse and transfer files on connected Android devices from Dolphin and other KDE applications. The worker uses the Android shell user's filesystem permissions.

## Features

- Browse directories and inspect file metadata, including size, permissions, and modification time.
- Download files and upload directly to the device with overwrite support.
- Create directories, rename or move files and directories, and permanently delete files and empty directories.

## Packages

- **Ubuntu:** see the [deb build and installation instructions](deb/README.md).
- **Arch Linux and Arch-based systems:** see the [Arch package instructions](arch/README.md), or run `./arch/build.sh --install` after installing `base-devel`.

## Build on Ubuntu 26.04 LTS

```sh
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev libkf6kio-dev adb
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
```

Restart Dolphin after installing the worker.

## Connect and browse

1. Enable USB debugging on the phone, connect it, and approve the debugging prompt.
2. Open **Network → ADB** in Dolphin, or enter `adb:/` in the location bar.
3. Open a device to access its shared storage at `/sdcard/`.

Device URLs include the selected ADB identifier, for example `adb://emulator-5554/sdcard/` or
`adb://192.168.1.20:5555/sdcard/`. Identifiers that would change during URL parsing use an
`id-` host encoding. For an old bookmark such as `adb:/sdcard/`, select the device under
`adb:/` and bookmark the new URL.

Copy files between Dolphin windows to download or upload them. For deletions, use Dolphin's **Delete** action.

Uploads use a local temporary file, then push directly to the requested Android path. An interrupted push can leave a partial destination file. Upload resume is unsupported. The device needs a modern Android shell with Toybox utilities including `stat` and `mv -T`.

## Smoke test

Open a device under `adb:/` and create a folder under `Download/`. Copy a nonempty file and a zero-byte file into it, then copy both back to a different local folder and compare them with the originals. Repeat an upload to exercise the overwrite prompt. Rename the uploaded files, delete them, then delete the empty test folder.

## Automated emulator test

Install the Android Emulator and Platform-Tools in the SDK, and create an Android Virtual Device (AVD). With a single installed AVD, omit `KIO_ADB_TEST_AVD`.

```sh
cmake -S . -B build -DKIO_ADB_EMULATOR_TEST=ON
cmake --build build --parallel
ANDROID_SDK=/path/to/sdk KIO_ADB_TEST_AVD=test-avd \
    ctest --test-dir build --output-on-failure -R '^adb-emulator-(download|upload|upload-space|list|list-directory|mkdir|rmdir)$'
```

Each test erases the selected AVD’s internal user data with `-wipe-data`, boots headlessly, and waits for a direct ADB write and read-back in shared storage. The download and listing tests seed their own files with ADB. The directory listing test checks the exact filenames in any order and compares file sizes when reported. The upload tests read the KIO-uploaded file back with ADB, including a case with spaces in the filename. The transfer tests compare bytes with the local fixture. The mkdir test creates a directory through KIO, checks the duplicate-directory error, and verifies the directory with ADB.

Each test shuts down its emulator on exit. Set `KIO_ADB_TEST_PORT` to a free even console port if the default of 5580 is occupied. The emulator also needs the next port for ADB.

## License

LGPL-2.0-or-later. See [LICENSE](LICENSE).
