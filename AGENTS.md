# kio-adb

This project is a KDE Frameworks 6 KIO worker that exposes Android devices over ADB. The worker implementation and protocol metadata live in `src/`. Integration tests live in `tests/`, and Debian packaging lives in `deb/`.

## Build and test

```sh
cmake -S . -B build -DKIO_ADB_EMULATOR_TEST=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -E '^adb-emulator-'
```

The last command runs the mock-ADB integration tests and requires no Android device. Use these tests for routine validation.

Emulator tests start the selected AVD with `-wipe-data`, which erases its user data. Run them only against a disposable test AVD and select it explicitly with `KIO_ADB_TEST_AVD`.

## Changes to the worker

- Exercise behavior changes through KIO jobs. Add cases to the mock-ADB tests when they can reproduce the behavior.
- For transfer changes, cover the affected direction and check file contents as well as progress or errors.
- For path-handling changes, cover filenames with spaces or quotes. Reuse the existing remote-shell quoting helper and preserve device selection through the `adb://` URL.
- When examining solutions to a problem, try to avoid or minimize external dependencies. If external dependencies are not avoidable, also mention them in the packaging files, but always ask me if introducing a new dependency is allowed.

## GitGub Issue handling

- When starting to fix a GitHub issue, make sure it gets assigned to me. Also, create a new branch according to git-flow naming, and associate the issue with that branch.
- When fixing issues, create new tests if necessary, run the whole test suite, and if successful, commit the changes automatically.
- When done, create a merge request.
