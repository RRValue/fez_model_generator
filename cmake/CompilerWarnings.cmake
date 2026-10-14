# Project-wide compiler options. Third-party headers (Qt, Eigen, assimp, Catch2) are
# included through imported targets, i.e. as system/external include paths,
# and are compiled with warnings off (/external:W0), so /WX only applies to
# warnings from project code.

function(fez_set_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /WX
            /permissive-
            /utf-8
            /Zc:__cplusplus
            /external:anglebrackets
            /external:W0
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
