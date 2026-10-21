# How to build AppImage Controller

These instructions describe the project's current Debian/Ubuntu build dependencies and package build flow. The CI build script is the most complete reference for packaging.


## Download source

This step is only required if you would like to build the source from GitHub. *If you have a local copy, please skip this section.*

You can download the source either using Git or by manually downloading a tarball on the [Releases page](https://github.com/rmacordeiro/AppImageController/releases).

The branch can be customized by setting the `-b` or `--branch` argument. Select `stable` to download the current stable source or use `master` to get the most current unstable source containing the latest features.

```shell
git clone https://github.com/rmacordeiro/AppImageController.git 
cd AppImageLauncher
git submodule update --init --recursive
```


## Dependencies

Install the compiler, CMake, Qt 5, library development headers, and Debian packaging tools:

For Ubuntu/Debian systems, this might look like:

```bash
sudo apt update
sudo apt install \
  cmake build-essential git pkg-config \
  libcurl4-openssl-dev libfuse-dev desktop-file-utils \
  libglib2.0-dev libcairo2-dev libssl-dev libbsd-dev \
  qttools5-dev-tools qtbase5-dev qt5-qmake qtdeclarative5-dev \
  libarchive-dev libboost-filesystem-dev librsvg2-dev librsvg2-bin \
  liblzma-dev libgcrypt-dev argagg-dev nlohmann-json3-dev clang \
  libgpgme-dev libzstd-dev \
  automake autoconf libtool patch wget curl \
  python3-venv rpm rpm2cpio patchelf
```

The package build downloads linuxdeploy and its Qt plugin. It also installs the linuxdeploy native-package plugin into a Python virtual environment in its temporary build directory.

On amd64 systems, the build also needs a Clang cross-target toolchain and 32-bit libc headers for the 32-bit launcher preload library:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install clang libc6-dev:i386
```

The selected Clang toolchain must also have matching native and i386 GCC C++ development libraries. For Ubuntu 26.04 where Clang selects GCC 16, install:

```bash
sudo apt install g++-16 libstdc++-16-dev:i386 libgcc-16-dev:i386
```

## Build a Debian package

```shell
./ci/build.sh
```

The script builds the full version (including the updater) and places the generated `.deb`, `.rpm`, and portable AppImage in the current working directory. Native packages use the `appimagecontroller` package identifier and replace the legacy `appimagelauncher` package on upgrade. Install the `rpm` and `rpm2cpio` packages listed above. The build uses temporary storage and removes its temporary build directory when it exits. To place outputs in the parent directory, run the script from there using its full path, for example:

```bash
cd ..
CXXFLAGS='-include stdint.h' ./AppImageController/ci/build.sh
```

If compiling the bundled CPR dependency fails because `intptr_t` is undefined, build with the missing standard header included:

```bash
CXXFLAGS='-include stdint.h' ./ci/build.sh
```

To build Lite instead, run `BUILD_LITE=1 ./ci/build.sh`; this produces the Lite AppImage rather than the full AppImage and native packages.

Install the resulting package for local testing with:

```bash
sudo apt install ./appimagecontroller*.deb
```

The package build may need additional development libraries depending on the distribution. For the exact CI dependency list, see [`ci/install-deps.sh`](./ci/install-deps.sh).
