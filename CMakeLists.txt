cmake_minimum_required(VERSION 3.20 FATAL_ERROR) # Policies <= CMP0120 default to NEW

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

set(MKTORRENT_PROJECT_VERSION "1.1")
project(mktorrent
    VERSION "${MKTORRENT_PROJECT_VERSION}"
    DESCRIPTION "A simple command-line utility to create BitTorrent metainfo files, written in C"
    HOMEPAGE_URL "https://github.com/Rudde/mktorrent"
    LANGUAGES C
)

# set a default build type when none is specified for single-config generators
include(DefaultBuildType)
default_build_type_config("Release")

# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
# dependency version requirements
set(requiredOpenSSLVersion 1.1.1)

# feature definition and setup
include(FeatureSummary)
include(OptionUtils)

feature_option_setup(MKTORRENT_LARGE_FILES "Enable support for files and torrents > 2 GiB on certain OSes (like 32-bit Linux)" OFF)
feature_option_setup(MKTORRENT_LONG_OPTIONS "Enable long options, started with two dashes" ON)
feature_option_setup(MKTORRENT_NO_HASH_CHECK "Disable checking if amount of bytes read for file hashing matches initially reported sizes\n(useless unless, for some strange reason, you change files yet to be hashed while mktorrent is running)" OFF)
feature_option_setup(MKTORRENT_OPENSSL "Use OpenSSL's SHA-1 implementation instead of compiling our own" ON)
feature_option_setup(MKTORRENT_PTHREADS "Use multiple POSIX threads for calculating hashes - should be much faster in systems with multiple CPU cores and fast storage" ON)
feature_option_setup(MKTORRENT_USE_GITREV_VERSION "Use a version string with git revision information, if available\n(note that git revision information is only updated on re-configure)" OFF)
feature_option_setup(MKTORRENT_VERBOSE_CONFIGURE "Show information about PACKAGES_FOUND and PACKAGES_NOT_FOUND in the configure output\n(only useful for debugging the CMake build scripts)" OFF)
tunable_setup(MKTORRENT_MAX_OPENFD 100 "Maximum number of simultaneously opened files descriptors when scanning a directory" TUNABLES_LIST)
tunable_setup(MKTORRENT_PROGRESS_PERIOD 200000 "Progress report update interval when hashing multithreaded, in μs" TUNABLES_LIST)

if (MKTORRENT_OPENSSL)
    find_package(OpenSSL ${requiredOpenSSLVersion} REQUIRED)
endif()

if (MKTORRENT_PTHREADS)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if (MKTORRENT_USE_GITREV_VERSION)
    include(GitRevisionString)
    make_git_revision_string(${MKTORRENT_PROJECT_VERSION} MKTORRENT_GITREV_VERSION)
endif()

add_executable(mktorrent)

target_sources(mktorrent
    PRIVATE
        # headers
        export.h
        ftw.h
        hash.h
        init.h
        ll.h
        mktorrent.h
        msg.h
        output.h
        $<$<NOT:$<BOOL:${MKTORRENT_OPENSSL}>>:sha1.h>

        # sources
        ftw.c
        init.c
        ll.c
        main.c
        msg.c
        output.c
        $<IF:$<BOOL:${MKTORRENT_PTHREADS}>,hash_pthreads.c,hash.c>
        $<$<NOT:$<BOOL:${MKTORRENT_OPENSSL}>>:sha1.c>
)

target_compile_features(mktorrent
    PRIVATE
        c_std_99
        c_function_prototypes
        c_variadic_macros
)

target_compile_definitions(mktorrent
    PRIVATE
        $<IF:$<BOOL:${MKTORRENT_USE_GITREV_VERSION}>,VERSION="${MKTORRENT_GITREV_VERSION}",VERSION="${MKTORRENT_PROJECT_VERSION}">
        $<$<BOOL:${MKTORRENT_PTHREADS}>:USE_PTHREADS>
        $<$<BOOL:${MKTORRENT_OPENSSL}>:USE_OPENSSL>
        $<$<BOOL:${MKTORRENT_LONG_OPTIONS}>:USE_LONG_OPTIONS>
        $<$<BOOL:${MKTORRENT_LARGE_FILES}>:_LARGEFILE_SOURCE _FILE_OFFSET_BITS=64>
        $<$<BOOL:${MKTORRENT_NO_HASH_CHECK}>:NO_HASH_CHECK>
        $<IF:$<EQUAL:${MKTORRENT_MAX_OPENFD},100>,MAX_OPENFD=100,MAX_OPENFD=${MKTORRENT_MAX_OPENFD}>
        $<IF:$<EQUAL:${MKTORRENT_PROGRESS_PERIOD},200000>,PROGRESS_PERIOD=200000,PROGRESS_PERIOD=${MKTORRENT_PROGRESS_PERIOD}>
)

target_compile_options(mktorrent
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<BOOL:${MKTORRENT_PTHREADS}>:-pthread>
)

target_link_libraries(mktorrent
    PRIVATE
        $<$<BOOL:${MKTORRENT_OPENSSL}>:OpenSSL::Crypto>
        $<$<BOOL:${MKTORRENT_PTHREADS}>:Threads::Threads>
)

target_link_options(mktorrent
    PRIVATE
        $<$<OR:$<BOOL:${MINGW}>,$<BOOL:${MSYS}>>:-static>
        $<$<BOOL:${MKTORRENT_PTHREADS}>:-pthread>
)

include(GNUInstallDirs)
install(TARGETS mktorrent RUNTIME COMPONENT runtime)

set(VERSION_MSG "Project version string: ")
if (MKTORRENT_USE_GITREV_VERSION)
    message(STATUS "${VERSION_MSG}\"${MKTORRENT_GITREV_VERSION}\"")
else()
    message(STATUS "${VERSION_MSG}\"${MKTORRENT_PROJECT_VERSION}\"")
endif()
if (MKTORRENT_VERBOSE_CONFIGURE)
    feature_summary(WHAT ALL)
else()
    feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
endif()
print_tunables(TUNABLES_LIST)
