/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <Network/Network.h> for the Xcode-SDK-free macOS target
(*-apple-macosx+open). Network.framework is closed; this reconstructs only the
nw_path_monitor + nw_path connectivity surface the window backend's network
reachability probe uses. The nw objects are treated as opaque handles (retained
via the framework's own ref-counting); symbols resolve from the baked Network.tbd.
**/

#ifndef __SPRT_OPEN_NETWORK_H_
#define __SPRT_OPEN_NETWORK_H_

#include <sys/cdefs.h>
#include <stdbool.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

/* nw objects — opaque handles. (The real framework makes them OS objects; treated
   here as plain pointers, which is ABI-compatible and compiles under ARC.) */
typedef struct nw_path *nw_path_t;
typedef struct nw_path_monitor *nw_path_monitor_t;
typedef struct nw_interface *nw_interface_t;

typedef enum {
	nw_path_status_invalid     = 0,
	nw_path_status_satisfied   = 1,
	nw_path_status_unsatisfied = 2,
	nw_path_status_satisfiable = 3,
} nw_path_status_t;

typedef enum {
	nw_interface_type_other    = 0,
	nw_interface_type_wifi     = 1,
	nw_interface_type_cellular = 2,
	nw_interface_type_wired    = 3,
	nw_interface_type_loopback = 4,
} nw_interface_type_t;

typedef void (^nw_path_monitor_update_handler_t)(nw_path_t path);
typedef void (^nw_path_monitor_cancel_handler_t)(void);

/* path queries */
extern nw_path_status_t nw_path_get_status(nw_path_t path);
extern bool nw_path_uses_interface_type(nw_path_t path, nw_interface_type_t interface_type);
extern bool nw_path_has_dns(nw_path_t path);
extern bool nw_path_has_ipv4(nw_path_t path);
extern bool nw_path_has_ipv6(nw_path_t path);
extern bool nw_path_is_expensive(nw_path_t path);
extern bool nw_path_is_constrained(nw_path_t path);

/* path monitor */
extern nw_path_monitor_t nw_path_monitor_create(void);
extern nw_path_monitor_t nw_path_monitor_create_with_type(nw_interface_type_t required_interface_type);
extern void nw_path_monitor_set_queue(nw_path_monitor_t monitor, dispatch_queue_t queue);
extern void nw_path_monitor_set_update_handler(nw_path_monitor_t monitor,
		nw_path_monitor_update_handler_t update_handler);
extern void nw_path_monitor_set_cancel_handler(nw_path_monitor_t monitor,
		nw_path_monitor_cancel_handler_t cancel_handler);
extern void nw_path_monitor_start(nw_path_monitor_t monitor);
extern void nw_path_monitor_cancel(nw_path_monitor_t monitor);

/* nw object retain/release (OS-object ref-counting) */
extern void nw_retain(void *obj);
extern void nw_release(void *obj);

__END_DECLS

#endif /* __SPRT_OPEN_NETWORK_H_ */
