/* Linker script for a Tau user module, installed as share/app-<arch>.lds.
 *
 * Symbol definitions only, no SECTIONS: lld keeps its own layout (R / RX / RW
 * PT_LOADs, PT_TLS, PT_GNU_EH_FRAME, RELRO, .init_array brackets). Base address,
 * page size and entry are link flags in make/os/tau-user.mk.
 *
 * libunwind's baremetal path finds the unwind tables through these symbols
 * instead of dl_iterate_phdr.
 */

PROVIDE_HIDDEN(__eh_frame_start = ADDR(.eh_frame));
PROVIDE_HIDDEN(__eh_frame_end = ADDR(.eh_frame) + SIZEOF(.eh_frame));

/* And the binary search table in front of it. This pair is why the baremetal
 * path costs nothing: with _LIBUNWIND_SUPPORT_DWARF_INDEX (which baremetal mode
 * turns on) the unwinder binary-searches .eh_frame_hdr and never scans FDEs
 * linearly. Without these two symbols it would. */
PROVIDE_HIDDEN(__eh_frame_hdr_start = ADDR(.eh_frame_hdr));
PROVIDE_HIDDEN(__eh_frame_hdr_end = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr));
