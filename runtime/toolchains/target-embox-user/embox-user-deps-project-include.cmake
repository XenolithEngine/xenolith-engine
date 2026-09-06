# Injected via -DCMAKE_PROJECT_INCLUDE into every Embox EL0 third-party dependency
# build (common/configure.mk Embox EL0 branch). It runs at the END of each project()
# call — i.e. AFTER enable_language(CXX) has loaded the Clang-CXX compiler module
# and populated the CMAKE_CXX<n>_STANDARD_COMPILE_OPTION maps (which is why the
# same overrides in the toolchain file do not stick: the module overwrites them).
#
# The sprt STL headers require C++20 (concepts, ...). Some C++ deps hardcode an
# older standard — harfbuzz does `set(CMAKE_CXX_STANDARD 11)` (then 17) and even
# appends `-std=c++11` to CMAKE_CXX_FLAGS — and cmake emits the selected standard's
# flag LAST, so it wins over any -std we place in CMAKE_CXX_FLAGS_INIT. Repoint every
# standard level to gnu++2a here so the flag cmake emits last is always C++20,
# regardless of what standard the dependency asks for. C dependencies never consult
# these CXX maps, so this is a no-op for them.
set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=gnu++2a")
set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
