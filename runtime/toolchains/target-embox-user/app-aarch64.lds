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
 * section's address. libunwind's baremetal path scans between these two
 * (UnwindCursor.hpp, __eh_frame_start/__eh_frame_end) instead of looking for
 * PT_GNU_EH_FRAME through dl_iterate_phdr. Contour L4 replaces that path with
 * the ordinary dl_iterate_phdr one, at which point these become unused -- but
 * PROVIDE_HIDDEN means an unreferenced symbol costs nothing, so the file can
 * stay until then and be deleted in one go.
 */

PROVIDE_HIDDEN(__eh_frame_start = ADDR(.eh_frame));
PROVIDE_HIDDEN(__eh_frame_end = ADDR(.eh_frame) + SIZEOF(.eh_frame));

/* And the binary search table in front of it. libunwind's baremetal path looks
 * for BOTH pairs: it uses the header's index when it is there and falls back to
 * a linear scan of .eh_frame when it is not. */
PROVIDE_HIDDEN(__eh_frame_hdr_start = ADDR(.eh_frame_hdr));
PROVIDE_HIDDEN(__eh_frame_hdr_end = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr));
