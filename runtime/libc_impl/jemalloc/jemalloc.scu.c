#define JEMALLOC_EXPORT
#define __SPRT_MSC_COMPAT

#include <unistd.h>

#ifdef _WIN32
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/message_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/time_api.h>
#include <sprt/wrappers/windows/context_api.h>
#include <sprt/wrappers/windows/dl_api.h>
#endif

#include "src/arena.c"
#include "src/background_thread.c"
#include "src/base.c"
#include "src/bin.c"
#include "src/bin_info.c"
#include "src/bitmap.c"
#include "src/buf_writer.c"
#include "src/cache_bin.c"
#include "src/ckh.c"
#include "src/conf.c"
#include "src/counter.c"
#include "src/ctl.c"
#include "src/decay.c"
#include "src/div.c"
#include "src/ecache.c"
#include "src/edata.c"
#include "src/edata_cache.c"
#include "src/ehooks.c"
#include "src/emap.c"
#include "src/eset.c"
#include "src/exp_grow.c"
#include "src/extent.c"
#include "src/extent_dss.c"
#include "src/extent_mmap.c"
#include "src/fxp.c"
#include "src/hook.c"
#include "src/hpa.c"
#include "src/hpa_central.c"
#include "src/hpa_hooks.c"
#include "src/hpa_utils.c"
#include "src/hpdata.c"
#include "src/inspect.c"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wunused-function"
#include "src/jemalloc.c"
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#include "src/large.c"
#pragma clang diagnostic pop

#include "src/log.c"
#include "src/malloc_io.c"
#include "src/mutex.c"
#include "src/nstime.c"
#include "src/pa.c"
#include "src/pac.c"
#include "src/pa_extra.c"
#include "src/pages.c"
#include "src/peak_event.c"
#include "src/prof.c"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "src/prof_data.c"
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-enum-forward-reference"
#include "src/prof_log.c"
#pragma clang diagnostic pop

#include "src/prof_recent.c"
#include "src/prof_stack_range.c"
#include "src/prof_stats.c"
#include "src/prof_sys.c"
#include "src/psset.c"
#include "src/rtree.c"
#include "src/safety_check.c"
#include "src/san_bump.c"
#include "src/san.c"
#include "src/sc.c"
#include "src/sec.c"
#include "src/stats.c"
#include "src/sz.c"
#include "src/tcache.c"
#include "src/test_hooks.c"
#include "src/thread_event.c"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#include "src/thread_event_registry.c"
#pragma clang diagnostic pop

#include "src/ticker.c"
#include "src/tsd.c"
#include "src/util.c"
#include "src/witness.c"

//#include "src/zone.c"

__SPRT_C_FUNC void *__SPRT_ID(malloc_impl)(size_t size) { return malloc(size); }

__SPRT_C_FUNC void *__SPRT_ID(calloc_impl)(size_t n, size_t size) { return calloc(n, size); }

__SPRT_C_FUNC void *__SPRT_ID(realloc_impl)(void *ptr, size_t size) { return realloc(ptr, size); }

__SPRT_C_FUNC void __SPRT_ID(free_impl)(void *ptr) { return free(ptr); }
