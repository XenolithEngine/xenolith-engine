/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <bsm/audit_session.h> for *-apple-macosx+open. The audit-session
 * ioctl/helper header ships in the SDK but has no apple-oss source (libbsm's
 * session half was never published). Consumers in this sysroot (lldb Host.mm)
 * include it but use only <bsm/audit.h> types (auditinfo_addr_t/getaudit_addr),
 * so a compile-compatible shim that pulls the real audit definitions is enough.
 * Extend with the au_sdev_* API only if a consumer actually needs it. */
#ifndef __SPRT_OPEN_BSM_AUDIT_SESSION_H_
#define __SPRT_OPEN_BSM_AUDIT_SESSION_H_

#include <bsm/audit.h>

#endif /* __SPRT_OPEN_BSM_AUDIT_SESSION_H_ */
