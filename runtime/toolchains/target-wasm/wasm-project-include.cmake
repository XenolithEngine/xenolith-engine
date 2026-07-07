# Included as the last step of every project() call in the wasm runtimes build
# (via -DCMAKE_PROJECT_INCLUDE). The Generic platform sets
# TARGET_SUPPORTS_SHARED_LIBS=FALSE, which makes cmake demote the runtimes'
# always-defined `*_shared` targets (unwind_shared, cxx_shared, cxxabi_shared —
# each EXCLUDE_FROM_ALL when *_ENABLE_SHARED=Off) to STATIC archives. Those then
# collide with the real static libs ("multiple rules generate lib/libunwind.a").
# Re-enable shared-lib SUPPORT so the excluded shared targets keep their distinct
# .so names; they are never actually built (EXCLUDE_FROM_ALL), so nothing links a
# real wasm shared object.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
