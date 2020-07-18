############################################
# Utilities for configurable build options #
############################################

#
# Functions for coupling add_feature_info() or CMAKE_DEPENDENT_OPTION() and option()
#

function(feature_option_setup _name _description _default)
    string(CONCAT _desc "${_description} (default: ${_default})")
    option("${_name}" "${_desc}" "${_default}")
    add_feature_info("${_name}" "${_name}" "${_desc}")
endfunction()

include(CMakeDependentOption)
function(feature_option_dependent_setup _name _description _default_opt _dependency _default_dep_not_sat)
    string(CONCAT _desc "${_description} (default: ${_default_opt}; depends on condition: ${_dependency})")
    CMAKE_DEPENDENT_OPTION("${_name}" "${_desc}" "${_default_opt}" "${_dependency}" "${_default_dep_not_sat}")
    add_feature_info("${_name}" "${_name}" "${_desc}")
endfunction()

#
# Functions for defining tunable variables and store and display their help text
#

# Define a tunable variable and append its help text to a list of help entries given by _tunables_help_list_name
function(tunable_setup _name _default _description _tunables_help_list_name)

    set("${_name}" "${_default}" CACHE STRING "${_description}")
    set(_tunable_help_text "${_name}: ${_description} (default: ${_default})")

    list(APPEND ${_tunables_help_list_name} ${_tunable_help_text})
    set(${_tunables_help_list_name} ${${_tunables_help_list_name}} PARENT_SCOPE)

endfunction()

# Print all tunable help entries stored in the list given by _tunables_help_list_name
function(print_tunables _tunables_help_list_name)

    # parent-scope list remains unchanged after these operations
    list(TRANSFORM ${_tunables_help_list_name} APPEND "\n" AT -1)
    list(TRANSFORM ${_tunables_help_list_name} PREPEND "* ")
    list(PREPEND ${_tunables_help_list_name} "Tunables:\n")

    foreach(_element IN LISTS ${_tunables_help_list_name})
        message(STATUS "${_element}")
    endforeach()

endfunction()
