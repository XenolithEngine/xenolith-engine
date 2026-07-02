// Umbrella function list for <fenv.h> — a re-includable subunit (no guard).
// The includer defines SPRT_FUNC_BEGIN / SPRT_FUNC_END / SPRT_FUNC_BODY and
// provides the fexcept_t / fenv_t types.

SPRT_FUNC_BEGIN
int feclearexcept(int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_feclearexcept(v);
}
#endif

SPRT_FUNC_BEGIN
int fegetexceptflag(fexcept_t *ex, int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fegetexceptflag(ex, v);
}
#endif
SPRT_FUNC_BEGIN
int feraiseexcept(int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_feraiseexcept(v);
}
#endif

SPRT_FUNC_BEGIN
int fesetexceptflag(const fexcept_t *ex, int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fesetexceptflag(ex, v);
}
#endif
SPRT_FUNC_BEGIN
int fetestexcept(int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fetestexcept(v);
}
#endif


SPRT_FUNC_BEGIN
int fegetround(void) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fegetround();
}
#endif

SPRT_FUNC_BEGIN
int fesetround(int v) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fesetround(v);
}
#endif


SPRT_FUNC_BEGIN
int fegetenv(fenv_t *ex) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fegetenv((fenv_t *)ex);
}
#endif

SPRT_FUNC_BEGIN
int feholdexcept(fenv_t *ex) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_feholdexcept((fenv_t *)ex);
}
#endif

SPRT_FUNC_BEGIN
int fesetenv(const fenv_t *ex) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_fesetenv((const fenv_t *)ex);
}
#endif

SPRT_FUNC_BEGIN
int feupdateenv(const fenv_t *ex) SPRT_FUNC_END
#if SPRT_FUNC_BODY
{
	return __sprt_feupdateenv((const fenv_t *)ex);
}
#endif
