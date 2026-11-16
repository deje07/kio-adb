# Build RPMs with openSUSE Build Service

Create a package named `kio-adb` in your OBS project. Add openSUSE
Tumbleweed and the Fedora release you want to support as build targets.
Each target builds against its own Qt and KDE Frameworks libraries.
Add other releases after verifying their builds.

For a Git-backed OBS package, select the branch containing these files and
set the SCM sync URL parameter `subdir=rpm`. OBS will then see
`kio-adb.spec`, `kio-adb.changes`, and `_service` at the package root.
The source service checks out the release tag, and the build-time tar
service creates the archive named in the spec.

For a new release, update the spec version, the revision and version in
`_service`, and the changelog. Install the resulting RPM with
`sudo zypper install ./<package>.rpm` on openSUSE or
`sudo dnf install ./<package>.rpm` on Fedora, using the actual filename.
Restart Dolphin and open `adb:/` with an authorized Android device connected.
