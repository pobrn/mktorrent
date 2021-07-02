cmake_minimum_required(VERSION 3.20 FATAL_ERROR) # Configurable policies: <= CMP0120

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

project(mktorrent
    VERSION 1.1
    DESCRIPTION "A simple command-line utility to create BitTorrent metainfo files, written in C"
    HOMEPAGE_URL "https://github.com/Rudde/mktorrent"
    LANGUAGES C
)

# set a default build type when none is specified for single-config generators
include(DefaultBuildType)
set_default_build_type(DEFAULT_BUILD_TYPE "Release")

get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(isMultiConfig)
    set(MKTORRENT_BUILD_CONFIG "$<CONFIG>")
else()
    set(MKTORRENT_BUILD_CONFIG "${CMAKE_BUILD_TYPE}")
endif()

# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
# dependency version requirements
set(requiredOpenSSLVersion "1.1.1")

# feature definition and setup
include(FeatureSummary)
include(OptionUtils)

feature_option_setup(
    NAME MKTORRENT_LONG_OPTIONS
    DESCRIPTION "Enable long options, started with two dashes"
    DEFAULT ON
)
feature_option_setup(
    NAME MKTORRENT_NO_HASH_CHECK
    DESCRIPTION "Disable checking if amount of bytes read for file hashing matches initially reported sizes\n(useless unless, for some strange reason, you change files yet to be hashed while mktorrent is running)"
    DEFAULT OFF
)
feature_option_setup(
    NAME MKTORRENT_OPENSSL
    DESCRIPTION "Use OpenSSL's SHA-1 implementation instead of compiling our own"
    DEFAULT ON
)
feature_option_setup(
    NAME MKTORRENT_PTHREADS
    DESCRIPTION "Use multiple POSIX threads for calculating hashes - should be much faster in systems with multiple CPU cores and fast storage"
    DEFAULT ON
)
feature_option_setup(
    NAME MKTORRENT_USE_GITREV_VERSION
    DESCRIPTION "Use a version string with git revision information, if available\n(note that git revision information is only updated on re-configure)"
    DEFAULT OFF
)
feature_option_setup(
    NAME MKTORRENT_VERBOSE_CONFIGURE
    DESCRIPTION "Show information about PACKAGES_FOUND and PACKAGES_NOT_FOUND in the configure output\n(only useful for debugging the CMake build scripts)"
    DEFAULT OFF
)
feature_tunable_setup(
    NAME MKTORRENT_MAX_OPENFD
    DESCRIPTION "Maximum number of simultaneously opened files descriptors when scanning a directory"
    DEFAULT 100
    LIST TUNABLES_LIST
)
feature_tunable_setup(
    NAME MKTORRENT_PROGRESS_PERIOD
    DESCRIPTION "Progress report update interval when hashing multithreaded, in μs"
    DEFAULT 200000
    LIST TUNABLES_LIST
)

if(MKTORRENT_OPENSSL)
    find_package(OpenSSL "${requiredOpenSSLVersion}" REQUIRED)
endif()

if(MKTORRENT_PTHREADS)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if(MKTORRENT_USE_GITREV_VERSION)
    include(GitRevisionString)
    make_git_revision_string(
        BASE_VERSION "${PROJECT_VERSION}"
        OUT_VAR MKTORRENT_GITREV_VERSION
    )
endif()

add_executable(mktorrent)

target_sources(mktorrent
    PRIVATE
        # headers
        src/export.h
        src/ftw.h
        src/hash.h
        src/init.h
        src/ll.h
        src/mktorrent.h
        src/msg.h
        src/output.h
        $<$<NOT:$<BOOL:${MKTORRENT_OPENSSL}>>:src/sha1.h>

        # sources
        src/ftw.c
        src/init.c
        src/ll.c
        src/main.c
        src/msg.c
        src/output.c
        $<IF:$<BOOL:${MKTORRENT_PTHREADS}>,src/hash_pthreads.c,src/hash.c>
        $<$<NOT:$<BOOL:${MKTORRENT_OPENSSL}>>:src/sha1.c>
)

target_compile_features(mktorrent
    PRIVATE
        c_std_99
        c_function_prototypes
        c_variadic_macros
)

target_compile_definitions(mktorrent
    PRIVATE
        _FILE_OFFSET_BITS=64
        $<IF:$<BOOL:${MKTORRENT_USE_GITREV_VERSION}>,VERSION="${MKTORRENT_GITREV_VERSION}",VERSION="${PROJECT_VERSION}">
        $<$<BOOL:${MKTORRENT_PTHREADS}>:USE_PTHREADS>
        $<$<BOOL:${MKTORRENT_OPENSSL}>:USE_OPENSSL>
        $<$<BOOL:${MKTORRENT_LONG_OPTIONS}>:USE_LONG_OPTIONS>
        $<$<BOOL:${MKTORRENT_NO_HASH_CHECK}>:NO_HASH_CHECK>
        $<IF:$<EQUAL:${MKTORRENT_MAX_OPENFD},100>,MAX_OPENFD=100,MAX_OPENFD="${MKTORRENT_MAX_OPENFD}">
        $<IF:$<EQUAL:${MKTORRENT_PROGRESS_PERIOD},200000>,PROGRESS_PERIOD=200000,PROGRESS_PERIOD="${MKTORRENT_PROGRESS_PERIOD}">
        BUILD_CFG="${MKTORRENT_BUILD_CONFIG}"
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

set(VERSION_MSG "${PROJECT_VERSION}")
if(MKTORRENT_USE_GITREV_VERSION)
    set(VERSION_MSG "${MKTORRENT_GITREV_VERSION}")
endif()
message(STATUS "Project version string: \"${VERSION_MSG}\"")

if(MKTORRENT_VERBOSE_CONFIGURE)
    feature_summary(WHAT ALL)
else()
    feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
endif()
print_tunables(LIST TUNABLES_LIST)
