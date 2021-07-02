############################################
# Utilities for configurable build options #
############################################

#
# Functions for coupling add_feature_info() or CMAKE_DEPENDENT_OPTION() and option()
#

function(feature_option_setup)
    set(options "")
    set(oneValueArgs NAME DESCRIPTION DEFAULT)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    set(_desc "${ARG_DESCRIPTION} (default: ${ARG_DEFAULT})")
    option("${ARG_NAME}" "${_desc}" "${ARG_DEFAULT}")
    add_feature_info("${ARG_NAME}" "${ARG_NAME}" "${_desc}")
endfunction()

include(CMakeDependentOption)
function(feature_option_dependent_setup ARG_NAME ARG_DESCRIPTION ARG_DEFAULT ARG_DEPENDS ARG_INIT)
    set(options "")
    set(oneValueArgs NAME DESCRIPTION DEFAULT DEPENDS INIT)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    set(_desc "${ARG_DESCRIPTION} (default: ${ARG_DEFAULT}; depends on condition: ${ARG_DEPENDS})")
    CMAKE_DEPENDENT_OPTION("${ARG_NAME}" "${_desc}" "${ARG_DEFAULT}" "${ARG_DEPENDS}" "${ARG_INIT}")
    add_feature_info("${ARG_NAME}" "${ARG_NAME}" "${_desc}")
endfunction()

#
# Functions for defining tunable variables and store and display their help text
#

# Define a tunable variable and append its help text to a list of help entries given by LIST
function(feature_tunable_setup)
    set(options "")
    set(oneValueArgs NAME DESCRIPTION DEFAULT LIST)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    set(_tunable_help_text "${ARG_NAME}: ${ARG_DESCRIPTION} (default: ${ARG_DEFAULT})")
    set("${ARG_NAME}" "${ARG_DEFAULT}" CACHE STRING "${ARG_DESCRIPTION}")

    list(APPEND "${ARG_LIST}" "${_tunable_help_text}")
    set("${ARG_LIST}" "${${ARG_LIST}}" PARENT_SCOPE)
endfunction()

# Print all tunable features help entries stored in the list given by LIST
function(print_tunables)
    set(options "")
    set(oneValueArgs LIST)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    # parent-scope list remains unchanged after these operations
    list(TRANSFORM "${ARG_LIST}" APPEND "\n" AT -1)
    list(TRANSFORM "${ARG_LIST}" PREPEND "* ")
    list(PREPEND "${ARG_LIST}" "Tunables:\n")

    foreach(_element IN LISTS "${ARG_LIST}")
        message(STATUS "${_element}")
    endforeach()
endfunction()
