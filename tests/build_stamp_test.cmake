# Runs cmake/BuildStamp.cmake on made-up `git describe` results and checks what each build
# would show (cmake -DSCRIPT=.../BuildStamp.cmake -DWORK=<folder> -P build_stamp_test.cmake).
function(expect_stamp describe build detail)
    set(output "${WORK}/stamp.hpp")
    file(REMOVE "${output}")
    execute_process(COMMAND ${CMAKE_COMMAND} -DVERSION=0.2.3 -DSOURCE_DIR=${WORK}
        "-DDESCRIBE=${describe}" -DOUTPUT=${output} -P ${SCRIPT} RESULT_VARIABLE failed)
    if(failed)
        message(FATAL_ERROR "BuildStamp failed for \"${describe}\"")
    endif()
    file(READ "${output}" text)
    if(NOT text MATCHES "PEERCHALK_BUILD \"${build}\"")
        message(FATAL_ERROR "\"${describe}\" should build \"${build}\":\n${text}")
    endif()
    if(NOT text MATCHES "PEERCHALK_BUILD_DETAIL \"${detail}\"")
        message(FATAL_ERROR "\"${describe}\" should detail \"${detail}\":\n${text}")
    endif()
endfunction()

expect_stamp("v0.2.3-0-gabc1234" "0\\.2\\.3" "0\\.2\\.3 \\(commit abc1234\\)")
expect_stamp("v0.2.3-7-gabc1234" "0\\.2\\.3\\+7" "0\\.2\\.3\\+7 \\(commit abc1234\\)")
expect_stamp("v0.2.3-0-gabc1234-dirty" "0\\.2\\.3" "0\\.2\\.3 \\(commit abc1234, with uncommitted changes\\)")
expect_stamp("v0.2.2-12-gabc1234" "0\\.2\\.3\\+12" "0\\.2\\.3\\+12 \\(commit abc1234\\)")
expect_stamp("" "0\\.2\\.3" "0\\.2\\.3") # no git: a source tarball
