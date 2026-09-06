// No arch-specific configuration: every HAVE_* gate for this platform is a
// property of the syscall table, not of aarch64, so they all live one level up
// in embox_user_sprt/config.h. The file must exist - cross/__sprt_config.h
// includes it unconditionally.
