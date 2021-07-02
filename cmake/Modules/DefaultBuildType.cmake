# Function that sets the build type for single-config generators to a specified default
# in case none is specified in the command line
# Has no effect on multi-config generators.

function(set_default_build_type)
    set(options "")
    set(oneValueArgs DEFAULT_BUILD_TYPE)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to ${ARG_DEFAULT_BUILD_TYPE} as none was specified.")
        set(CMAKE_BUILD_TYPE "${ARG_DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
        # Set the possible values of build type for cmake-gui, ccmake
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    endif()
endfunction()
