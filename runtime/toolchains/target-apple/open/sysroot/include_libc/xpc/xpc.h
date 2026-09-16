/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <xpc/xpc.h> for *-apple-macosx+open. libxpc has no apple-oss
 * source; reconstruct ONLY the surface lldb's Host.mm (the XPC root-debugging
 * launcher path) uses: the C object/connection types, the error/type globals,
 * the dictionary accessors, and the connection lifecycle calls. Types and
 * signatures match the SDK <xpc/xpc.h> C (non-OS_OBJECT) branch; every symbol
 * is a real libSystem export (libxpc is reexported by libSystem) baked into
 * libSystem.tbd. */
#ifndef __SPRT_OPEN_XPC_H_
#define __SPRT_OPEN_XPC_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

typedef struct _xpc_object_s *xpc_object_t;
typedef xpc_object_t xpc_connection_t;

struct _xpc_type_s;
typedef const struct _xpc_type_s *xpc_type_t;

extern const struct _xpc_type_s _xpc_type_error;
extern const struct _xpc_type_s _xpc_type_dictionary;
#define XPC_TYPE_ERROR      (&_xpc_type_error)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)

/* Error singletons (dictionaries) + the description key. */
extern const struct _xpc_object_s _xpc_error_connection_interrupted;
extern const struct _xpc_object_s _xpc_error_connection_invalid;
#define XPC_ERROR_CONNECTION_INTERRUPTED \
		((xpc_object_t)&_xpc_error_connection_interrupted)
#define XPC_ERROR_CONNECTION_INVALID \
		((xpc_object_t)&_xpc_error_connection_invalid)

extern const char *const _xpc_error_key_description;
#define XPC_ERROR_KEY_DESCRIPTION _xpc_error_key_description

typedef void (^xpc_handler_t)(xpc_object_t object);
typedef void (*xpc_finalizer_t)(void *value);

xpc_type_t xpc_get_type(xpc_object_t object);
void xpc_release(xpc_object_t object);

xpc_connection_t xpc_connection_create(const char *name, dispatch_queue_t targetq);
void xpc_connection_set_event_handler(xpc_connection_t connection, xpc_handler_t handler);
void xpc_connection_set_finalizer_f(xpc_connection_t connection, xpc_finalizer_t finalizer);
void xpc_connection_resume(xpc_connection_t connection);
void xpc_connection_set_context(xpc_connection_t connection, void *context);
void *xpc_connection_get_context(xpc_connection_t connection);
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t connection,
		xpc_object_t message);
/* async reply/barrier/cancel — intercepted by tsan, hence declared here too. */
void xpc_connection_send_message_with_reply(xpc_connection_t connection,
		xpc_object_t message, dispatch_queue_t replyq, xpc_handler_t handler);
void xpc_connection_send_barrier(xpc_connection_t connection, dispatch_block_t barrier);
void xpc_connection_cancel(xpc_connection_t connection);

xpc_object_t xpc_dictionary_create(const char *const *keys,
		const xpc_object_t *values, size_t count);
void xpc_dictionary_set_string(xpc_object_t xdict, const char *key, const char *string);
void xpc_dictionary_set_data(xpc_object_t xdict, const char *key,
		const void *bytes, size_t length);
void xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value);
void xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value);
const char *xpc_dictionary_get_string(xpc_object_t xdict, const char *key);
int64_t xpc_dictionary_get_int64(xpc_object_t xdict, const char *key);
uint64_t xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key);

__END_DECLS

#endif /* __SPRT_OPEN_XPC_H_ */
