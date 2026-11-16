# Build the Arch package

Run these commands from the repository root on Arch Linux or an Arch-based
system with KDE Frameworks 6 packages available:

```sh
sudo pacman -Syu --needed base-devel
./arch/build.sh --install
```

Run the script as your normal user. `makepkg` uses sudo to install missing build
and runtime dependencies, then builds and installs the package. Omit `--install`
to only build it. Additional makepkg options, such as `--force` for rebuilding
the same version, can be passed to the script.

The script stages the current checkout (including local changes) in a temporary
directory, adds its source checksum to the staged PKGBUILD, and writes packages
to `arch/dist/`. The PKGBUILD is intended for this local build workflow; it is
not an AUR listing and requires the source archive prepared by the script.
Build on the system you intend to install on so the package uses that system's
Qt and KDE libraries. The recipe allows x86_64 and aarch64 builds; each needs
its own native build environment.

To install an already built package, pass its exact filename to pacman:

```sh
sudo pacman -U ./arch/dist/kio-adb-0.3.1-1-x86_64.pkg.tar.zst
```

The filename depends on the architecture and makepkg compression settings.
Restart Dolphin and open `adb:/` with an authorized Android device connected.
See the [project README](../README.md) for device setup and a smoke test.

Remove the package with `sudo pacman -Rns kio-adb`.

The package follows Arch's [PKGBUILD format](https://man.archlinux.org/man/core/pacman/PKGBUILD.5.en).
