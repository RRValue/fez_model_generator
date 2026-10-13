# Project-wide compiler options. Third-party headers (Qt, Eigen, assimp) are
# included through imported targets, i.e. as system/external include paths,
# and are compiled with warnings off (/external:W0).

function(fez_set_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
            /external:anglebrackets
            /external:W0
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
