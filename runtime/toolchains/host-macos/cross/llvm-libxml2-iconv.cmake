# Injected into the target-llvm build via -DCMAKE_PROJECT_INCLUDE (runs right after
# every project() call, including sub-projects — keep it idempotent).
#
# The +open sysroot ships libxml2 as a STATIC usr/lib/libxml2.a whose objects
# reference libiconv; a plain find_package(LibXml2) builds an imported target
# without that dependency, so every consumer (llvm-mt/WindowsManifest, lldb)
# fails to link. Pre-seed the find results and create LibXml2::LibXml2 ourselves
# with libiconv.tbd attached — FindLibXml2 keeps an already-existing target
# (`if(NOT TARGET ...)`) and only fills in the version from xmlversion.h.
# The link-probe short-circuit lives on the command line (-DHAVE_LIBXML2=1):
# config-ix's check_symbol_exists would link libxml2.a WITHOUT the interface
# and fail.

if(CMAKE_SYSROOT AND NOT TARGET LibXml2::LibXml2)
	set(LIBXML2_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include/libxml2" CACHE PATH "" FORCE)
	set(LIBXML2_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libxml2.a" CACHE FILEPATH "" FORCE)

	find_library(SPRT_ICONV_LIBRARY NAMES iconv)
	if(NOT SPRT_ICONV_LIBRARY)
		set(SPRT_ICONV_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libiconv.tbd")
	endif()

	add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
	set_target_properties(LibXml2::LibXml2 PROPERTIES
		IMPORTED_LOCATION "${LIBXML2_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}"
		INTERFACE_LINK_LIBRARIES "${SPRT_ICONV_LIBRARY}")
endif()
