/* Linker script for an Embox EL0 application.
 *
 * Symbol definitions ONLY -- no SECTIONS command, deliberately. A script with no
 * SECTIONS keeps lld's own layout and merely adds these symbols; a script that
 * declared the layout would have to reproduce every segment lld already gets
 * right (separate R / RX / RW PT_LOADs, PT_TLS, PT_GNU_EH_FRAME, RELRO, the
 * .init_array brackets) and would drift from it at the first lld release.
 *
 * The base address, page size and entry point are link FLAGS instead, in
 * make/os/embox-user.mk: --image-base, -z max-page-size, -e. They belong there
 * because they are properties of the target, not of the section layout.
 *
 * What is left is the one thing flags cannot express: a symbol whose value is a
 * section's address. libunwind's baremetal path reads these
 * (AddressSpace.hpp, findUnwindSections) instead of looking for PT_GNU_EH_FRAME
 * through dl_iterate_phdr, and contour L4 decided that stays: the linker is the
 * one party that knows these bounds exactly, and there is only ever one object
 * to ask about (decision D5). See target-embox-user/compiler_rt.mk for the whole
 * argument.
 */

PROVIDE_HIDDEN(__eh_frame_start = ADDR(.eh_frame));
PROVIDE_HIDDEN(__eh_frame_end = ADDR(.eh_frame) + SIZEOF(.eh_frame));

/* And the binary search table in front of it. This pair is why the baremetal
 * path costs nothing: with _LIBUNWIND_SUPPORT_DWARF_INDEX (which baremetal mode
 * turns on) the unwinder binary-searches .eh_frame_hdr and never scans FDEs
 * linearly. Without these two symbols it would. */
PROVIDE_HIDDEN(__eh_frame_hdr_start = ADDR(.eh_frame_hdr));
PROVIDE_HIDDEN(__eh_frame_hdr_end = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr));
