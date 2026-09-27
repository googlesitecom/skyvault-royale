# ============================================================================
#  SkyVault Engine - politicas de warnings por compilador
# ============================================================================
function(sv_set_target_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4 /permissive- /MP
      /wd4100 # parametro sin usar en interfaces
      /wd4201 # unions sin nombre
      /wd4505 # funciones estaticas sin usar
    )
    if(SKYVAULT_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wshadow -Wno-unused-parameter
      -Wno-missing-field-initializers
      -Wno-unused-but-set-variable
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      target_compile_options(${target} PRIVATE -Wno-class-memaccess)
    endif()
    if(SKYVAULT_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
