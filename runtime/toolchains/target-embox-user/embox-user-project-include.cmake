# Included as the last step of every project() call in the Embox EL0 runtimes build
# (via -DCMAKE_PROJECT_INCLUDE). The Generic platform sets
# TARGET_SUPPORTS_SHARED_LIBS=FALSE, which makes cmake demote the runtimes'
# always-defined `*_shared` targets (unwind_shared, cxx_shared, cxxabi_shared —
# each EXCLUDE_FROM_ALL when *_ENABLE_SHARED=Off) to STATIC archives. Those then
# collide with the real static libs ("multiple rules generate lib/libunwind.a").
# Re-enable shared-lib SUPPORT so the excluded shared targets keep their distinct
# .so names; they are never actually built (EXCLUDE_FROM_ALL), so nothing links a
# real wasm shared object.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

# The LLVM runtimes ask for C++17 (llvm-project/runtimes sets CMAKE_CXX_STANDARD
# 17, and libunwind needs no more than that). The sprt STL headers they pull in
# through <string.h> need C++20 -- sprt/cxx/__type_traits/types.h declares
# `concept`s -- and cmake emits the selected standard's flag LAST, so a -std in
# CMAKE_CXX_FLAGS_INIT loses to it.
#
# Repoint every standard level at gnu++2a, exactly as the deps' project-include
# does for the same reason. This runs at the end of project(), i.e. after
# enable_language(CXX) populated these maps -- which is why the toolchain file
# cannot do it: the compiler module would overwrite it.
set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
