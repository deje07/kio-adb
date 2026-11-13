# Build the Ubuntu package

Run these commands from the repository root on Ubuntu 26.04 LTS or a later
Ubuntu release. Enable the Universe repository for the KDE and ADB packages.

```sh
sudo apt install build-essential debhelper dpkg-dev cmake extra-cmake-modules libkf6kio-dev qt6-base-dev
./deb/build.sh
```

The script stages the source in a temporary directory and writes the packages
and build metadata to `deb/dist/`. Build on the Ubuntu release and architecture
you intend to install on so the package uses that system's Qt and KDE libraries.

Install the package with APT, which also installs the required dependencies.

```sh
sudo apt install "./deb/dist/kio-adb_0.1.0_$(dpkg --print-architecture).deb"
```

Restart Dolphin and open `adb:/sdcard/` with an authorized Android device
connected. See the [project README](../README.md) for device setup.
