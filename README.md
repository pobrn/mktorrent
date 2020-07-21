# mktorrent

![Github Actions CI CD](https://github.com/FranciscoPombal/mktorrent/workflows/Github%20Actions%20CI%20CD/badge.svg)

mktorrent is a simple command-line utility to create BitTorrent metainfo files, written in C.

It should work on any POSIX-compliant OS. It can also run on Windows by building with MinGW.

## Build and install

The project is built with CMake.

Basic usage:

```sh
cd mktorrent
cmake -S . -B build   # configure build with default options
cmake --build build   # build
cmake --install build # (optional) install mktorrent to the system's default installation prefix
```

The defaults to build `mktorrent` are optimized for a modern 64-bit system and include OpenSSL (`MKTORRENT_OPENSSL`) and POSIX threads (`MKTORRENT_PTHREADS`) support, since this results in substantially better performance.
If the build fails due to your system not having the required packages, you can install them or disable one or both of these features at **configure-time**. Read more about the available **build options** below.

### Build options

The configure step will show `mktorrent`-specific options that can passed to the (re)configure command-line as `-DOPTION=<value>` to customize the build.
Any option not specified will use its default value.

| OPTION                       | DEFAULT | DESCRIPTION |
|------------------------------|---------|-------------|
| MKTORRENT_LONG_OPTIONS       | ON      | Enable long options, started with two dashes |
| MKTORRENT_NO_HASH_CHECK      | OFF     | Disable checking if amount of bytes read for file hashing matches initially reported sizes (useless unless, for some strange reason, you change files yet to be hashed while mktorrent is running) |
| MKTORRENT_OPENSSL            | ON      | Use OpenSSL's SHA-1 implementation instead of compiling our own |
| MKTORRENT_PTHREADS           | ON      | Use multiple POSIX threads for calculating hashes - should be much faster in systems with multiple CPU cores and fast storage |
| MKTORRENT_USE_GITREV_VERSION | OFF     | Use a version string with git revision information, if available (note that git revision information is only updated on re-configure) |
| MKTORRENT_VERBOSE_CONFIGURE  | OFF     | Show information about PACKAGES_FOUND and PACKAGES_NOT_FOUND in the configure output (only useful for debugging the CMake build scripts) |
| MKTORRENT_MAX_OPENFD         | 100     | Maximum number of simultaneously opened files descriptors when scanning a directory |
| MKTORRENT_PROGRESS_PERIOD    | 200000  | Progress report update interval when hashing multithreaded, in μs |

Example:

`cmake -S . -B build -DMKTORRENT_OPENSSL=OFF -DMKTORRENT_USE_GITREV_VERSION=ON`

### Building on Windows with MinGW

It's easy to build `mktorrent` on Windows with MinGW.

For example, using MSYS2+MinGW64 for a 64-bit build:

1. Install MSYS2, and run `pacman -Syuu` until there are no updates left.
2. Start a MinGW64 console
3. Install the prerequisites:

    `pacman -S mingw64/mingw-w64-x86_64-ninja mingw64/mingw-w64-x86_64-cmake mingw64/mingw-w64-x86_64-headers-git mingw64/mingw-w64-x86_64-gcc`

4. Optionally, install these as well:

    `pacman -S git mingw64/mingw-w64-x86_64-openssl`

5. Build as usual. You may want to link statically with OpenSSL. To do so, pass `-DOPENSSL_USE_STATIC_LIBS=ON` at **configure-time**.

### Notes and tips

- It is recommended to do out-of-tree builds. This is the case of the example above, where a `build/` directory is used for build artifacts.
- Build types:
  - When using single-configuration generators (`Unix Makefiles`, `Ninja`, ...), use `-DCMAKE_BUILD_TYPE=<cfg>` at **configure-time** to select the build configuration. If not specified, `Release` is used by default.
  - When using multi-configuration generators (`Ninja Multi-Config`, `Visual Studio 16 2019`, ...) use `--config <cfg>` at **build-time** to select the build configuration. If not specified, the default is to build the `Debug` configuration.
- Interesting CMake **configure-time** options:
  - Override the installation prefix with `-DCMAKE_INSTALL_PREFIX=<prefix>`. It can also be overriden at **install-time** with `--prefix <prefix>`. The default is `/usr` on Unix-like systems.
  - Pass `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` to generate a `compile_commands.json` "compilation database" file at the project root for use with language-intelligence tools such as `clangd`.
  - Pass `--graphviz=<file>` to generate a graph of the link dependencies of the project in `graphviz` format.

## More info

See the [wiki][link wiki] for more info on the project and documentation on build options.

[link wiki]: https://github.com/Rudde/mktorrent/wiki
