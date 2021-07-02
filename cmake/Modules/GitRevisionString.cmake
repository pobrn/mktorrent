# Function to append git revision information to a version string
# If there is no git repository, a FATAL_ERROR is raised
# The result is returned in the variable specified by OUT_VAR

function(make_git_revision_string)
    set(options "")
    set(oneValueArgs BASE_VERSION OUT_VAR)
    set(multiValueArgs "")

    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    set(_version "${ARG_BASE_VERSION}")

    execute_process(COMMAND "git" "rev-parse" "--short" "HEAD"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_REV
        ERROR_QUIET
    )

    if(NOT ("${GIT_REV}" STREQUAL ""))
        execute_process(
            COMMAND "git" "diff" "--quiet" "--exit-code"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULTS_VARIABLE IS_WORKING_DIR_DIRTY)
        execute_process(
            COMMAND "git" "describe" "--exact-match" "--tags"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
        execute_process(
            COMMAND "git" "rev-parse" "--abbrev-ref" "HEAD"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_BRANCH)
        string(STRIP "${GIT_REV}" GIT_REV)
        string(SUBSTRING "${GIT_REV}" 0 7 GIT_REV)
        set(GIT_DIFF "")
        if(IS_WORKING_DIR_DIRTY)
            set(GIT_DIFF "~dirty")
        endif()
        string(STRIP "${GIT_TAG}" GIT_TAG)
        string(STRIP "${GIT_BRANCH}" GIT_BRANCH)

        set(_version "${ARG_BASE_VERSION}-${GIT_REV}${GIT_DIFF} (branch: ${GIT_BRANCH}")

        if(NOT ("${GIT_TAG}" STREQUAL ""))
            string(APPEND _version " tag: ${GIT_TAG})")
        else()
            string(APPEND _version ")")
        endif()
    else()
        message(WARNING "Could not get git revision information, falling back to ${ARG_BASE_VERSION}")
    endif()

    set("${ARG_OUT_VAR}" "${_version}" PARENT_SCOPE)

endfunction()
