/*
 * RinOS libc - tgmath.h
 * C type-generic math dispatch for the math and complex interfaces provided
 * by this SDK.
 */

#ifndef _TGMATH_H
#define _TGMATH_H

#include "math.h"
#include "complex.h"

/* C++ has overloaded math interfaces of its own.  The C type-generic macros
 * below use _Generic and are defined only for C translation units. */
#ifndef __cplusplus

/* The controlling expression of _Generic is not evaluated.  For functions
 * with several generic parameters, the usual arithmetic conversions in the
 * controlling expression select the common real/complex precision. */
#define __RIN_TGMATH_UNARY_REAL(name, x) \
    _Generic((x), \
        float: name##f, \
        long double: name##l, \
        default: name)(x)

#define __RIN_TGMATH_UNARY_REAL_WITH_ARG(name, x, arg) \
    _Generic((x), \
        float: name##f, \
        long double: name##l, \
        default: name)((x), (arg))

#define __RIN_TGMATH_GENERIC_ARGUMENT(x) \
    _Generic((x), \
        float: (x), \
        double: (x), \
        long double: (x), \
        float _Complex: (x), \
        double _Complex: (x), \
        long double _Complex: (x), \
        default: (double)(x))

#define __RIN_TGMATH_UNARY_COMPLEX(name, x) \
    _Generic((x), \
        float: name##f, \
        double: name, \
        long double: name##l, \
        float _Complex: c##name##f, \
        double _Complex: c##name, \
        long double _Complex: c##name##l, \
        default: name)(x)

#define __RIN_TGMATH_BINARY_REAL(name, x, y) \
    _Generic((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(y)), \
        float: name##f, \
        long double: name##l, \
        default: name)(x, y)

#define __RIN_TGMATH_BINARY_REAL_WITH_ARG(name, x, y, arg) \
    _Generic((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(y)), \
        float: name##f, \
        long double: name##l, \
        default: name)(x, y, arg)

#define __RIN_TGMATH_BINARY_COMPLEX(name, x, y) \
    _Generic((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(y)), \
        float: name##f, \
        double: name, \
        long double: name##l, \
        float _Complex: c##name##f, \
        double _Complex: c##name, \
        long double _Complex: c##name##l, \
        default: name)(x, y)

#define __RIN_TGMATH_TERNARY_REAL(name, x, y, z) \
    _Generic(((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
               __RIN_TGMATH_GENERIC_ARGUMENT(y)) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(z)), \
        float: name##f, \
        long double: name##l, \
        default: name)(x, y, z)

/* C23 narrower arithmetic (B45): no f-suffixed spellings exist --
 * float pairs use the double spelling (which computes narrow and
 * returns float); only long-double involvement selects the l
 * spelling (which computes in double and returns double). */
#define __RIN_TGMATH_NARROW1(name, x) \
    _Generic((x), \
        long double: name##l, \
        default: name)(x)

#define __RIN_TGMATH_NARROW2(name, x, y) \
    _Generic((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(y)), \
        long double: name##l, \
        default: name)(x, y)

#define __RIN_TGMATH_NARROW3(name, x, y, z) \
    _Generic(((__RIN_TGMATH_GENERIC_ARGUMENT(x) + \
               __RIN_TGMATH_GENERIC_ARGUMENT(y)) + \
              __RIN_TGMATH_GENERIC_ARGUMENT(z)), \
        long double: name##l, \
        default: name)(x, y, z)

/* C23 conversions (B45): dispatch on x; rnd/width pass through. */
#define __RIN_TGMATH_FROMFP(name, x, rnd, width) \
    _Generic((x), \
        float: name##f, \
        long double: name##l, \
        default: name)((x), (rnd), (width))

/* Real/complex function pairs. */
#define acos(x)  __RIN_TGMATH_UNARY_COMPLEX(acos, (x))
#define acosh(x) __RIN_TGMATH_UNARY_COMPLEX(acosh, (x))
#define asin(x)  __RIN_TGMATH_UNARY_COMPLEX(asin, (x))
#define asinh(x) __RIN_TGMATH_UNARY_COMPLEX(asinh, (x))
#define atan(x)  __RIN_TGMATH_UNARY_COMPLEX(atan, (x))
#define atanh(x) __RIN_TGMATH_UNARY_COMPLEX(atanh, (x))
#define cos(x)   __RIN_TGMATH_UNARY_COMPLEX(cos, (x))
#define cosh(x)  __RIN_TGMATH_UNARY_COMPLEX(cosh, (x))
#define exp(x)   __RIN_TGMATH_UNARY_COMPLEX(exp, (x))
#define log(x)   __RIN_TGMATH_UNARY_COMPLEX(log, (x))
#define sin(x)   __RIN_TGMATH_UNARY_COMPLEX(sin, (x))
#define sinh(x)  __RIN_TGMATH_UNARY_COMPLEX(sinh, (x))
#define sqrt(x)  __RIN_TGMATH_UNARY_COMPLEX(sqrt, (x))
#define tan(x)   __RIN_TGMATH_UNARY_COMPLEX(tan, (x))
#define tanh(x)  __RIN_TGMATH_UNARY_COMPLEX(tanh, (x))
#define pow(x, y) __RIN_TGMATH_BINARY_COMPLEX(pow, (x), (y))

/* fabs is the shared generic spelling for fabs and cabs. */
#define fabs(x) \
    _Generic((x), \
        float: fabsf, \
        double: fabs, \
        long double: fabsl, \
        float _Complex: cabsf, \
        double _Complex: cabs, \
        long double _Complex: cabsl, \
        default: fabs)(x)

/* Real-only functions whose full float/double/long-double families are
 * present in the public math header.  Integer arguments select double. */
#define ceil(x)       __RIN_TGMATH_UNARY_REAL(ceil, (x))
#define cbrt(x)       __RIN_TGMATH_UNARY_REAL(cbrt, (x))
#define exp2(x)       __RIN_TGMATH_UNARY_REAL(exp2, (x))
#define expm1(x)      __RIN_TGMATH_UNARY_REAL(expm1, (x))
#define erf(x)        __RIN_TGMATH_UNARY_REAL(erf, (x))
#define erfc(x)       __RIN_TGMATH_UNARY_REAL(erfc, (x))
#define floor(x)      __RIN_TGMATH_UNARY_REAL(floor, (x))
#define ilogb(x)      __RIN_TGMATH_UNARY_REAL(ilogb, (x))
#define lgamma(x)     __RIN_TGMATH_UNARY_REAL(lgamma, (x))
#define llogb(x)      __RIN_TGMATH_UNARY_REAL(llogb, (x))
#define log10(x)      __RIN_TGMATH_UNARY_REAL(log10, (x))
#define log1p(x)      __RIN_TGMATH_UNARY_REAL(log1p, (x))
#define log2(x)       __RIN_TGMATH_UNARY_REAL(log2, (x))
#define logb(x)       __RIN_TGMATH_UNARY_REAL(logb, (x))
#define nearbyint(x)  __RIN_TGMATH_UNARY_REAL(nearbyint, (x))
#define nextdown(x)   __RIN_TGMATH_UNARY_REAL(nextdown, (x))
#define nextup(x)     __RIN_TGMATH_UNARY_REAL(nextup, (x))
#define rint(x)       __RIN_TGMATH_UNARY_REAL(rint, (x))
#define round(x)      __RIN_TGMATH_UNARY_REAL(round, (x))
#define lrint(x)      __RIN_TGMATH_UNARY_REAL(lrint, (x))
#define llrint(x)     __RIN_TGMATH_UNARY_REAL(llrint, (x))
#define lround(x)     __RIN_TGMATH_UNARY_REAL(lround, (x))
#define llround(x)    __RIN_TGMATH_UNARY_REAL(llround, (x))
#define roundeven(x)  __RIN_TGMATH_UNARY_REAL(roundeven, (x))
#define tgamma(x)     __RIN_TGMATH_UNARY_REAL(tgamma, (x))
#define trunc(x)      __RIN_TGMATH_UNARY_REAL(trunc, (x))

/* Multi-parameter functions with a common real precision. */
#define atan2(x, y)      __RIN_TGMATH_BINARY_REAL(atan2, (x), (y))
#define copysign(x, y)   __RIN_TGMATH_BINARY_REAL(copysign, (x), (y))
#define fdim(x, y)       __RIN_TGMATH_BINARY_REAL(fdim, (x), (y))
#define fma(x, y, z)     __RIN_TGMATH_TERNARY_REAL(fma, (x), (y), (z))
#define fmax(x, y)       __RIN_TGMATH_BINARY_REAL(fmax, (x), (y))
#define fmin(x, y)       __RIN_TGMATH_BINARY_REAL(fmin, (x), (y))
#define fmod(x, y)       __RIN_TGMATH_BINARY_REAL(fmod, (x), (y))
#define hypot(x, y)      __RIN_TGMATH_BINARY_REAL(hypot, (x), (y))
#define nextafter(x, y)  __RIN_TGMATH_BINARY_REAL(nextafter, (x), (y))
#define remainder(x, y)  __RIN_TGMATH_BINARY_REAL(remainder, (x), (y))
#define remquo(x, y, quo) \
    __RIN_TGMATH_BINARY_REAL_WITH_ARG(remquo, (x), (y), (quo))
#define scalbn(x, n)     __RIN_TGMATH_UNARY_REAL_WITH_ARG(scalbn, (x), (n))
#define scalbln(x, n)    __RIN_TGMATH_UNARY_REAL_WITH_ARG(scalbln, (x), (n))

/* C23 extrema (B44): usual-arithmetic-conversion dispatch. */
#define fmaximum(x, y)     __RIN_TGMATH_BINARY_REAL(fmaximum, (x), (y))
#define fminimum(x, y)     __RIN_TGMATH_BINARY_REAL(fminimum, (x), (y))
#define fmaximum_num(x, y) __RIN_TGMATH_BINARY_REAL(fmaximum_num, (x), (y))
#define fminimum_num(x, y) __RIN_TGMATH_BINARY_REAL(fminimum_num, (x), (y))
#define fmaximum_mag(x, y) __RIN_TGMATH_BINARY_REAL(fmaximum_mag, (x), (y))
#define fminimum_mag(x, y) __RIN_TGMATH_BINARY_REAL(fminimum_mag, (x), (y))
/* C23 canonicalize: dispatch on the source pointer's pointee. */
#define canonicalize(cx, x) \
    _Generic((x), float *: canonicalizef, const float *: canonicalizef, \
             long double *: canonicalizel, \
             const long double *: canonicalizel, \
             default: canonicalize)((cx), (x))

/* C23 logp1/exp2m1 (B45): ordinary real dispatch. */
#define logp1(x)   __RIN_TGMATH_UNARY_REAL(logp1, (x))
#define log10p1(x) __RIN_TGMATH_UNARY_REAL(log10p1, (x))
#define log2p1(x)  __RIN_TGMATH_UNARY_REAL(log2p1, (x))
#define exp2m1(x)  __RIN_TGMATH_UNARY_REAL(exp2m1, (x))

/* C23 narrower arithmetic (B45). */
#define fadd(x, y)    __RIN_TGMATH_NARROW2(fadd, (x), (y))
#define fsub(x, y)    __RIN_TGMATH_NARROW2(fsub, (x), (y))
#define fmul(x, y)    __RIN_TGMATH_NARROW2(fmul, (x), (y))
#define fdiv(x, y)    __RIN_TGMATH_NARROW2(fdiv, (x), (y))
#define ffma(x, y, z) __RIN_TGMATH_NARROW3(ffma, (x), (y), (z))
#define fsqrt(x)      __RIN_TGMATH_NARROW1(fsqrt, (x))

/* C23 conversions (B45). */
#define fromfp(x, rnd, width)     __RIN_TGMATH_FROMFP(fromfp, (x), (rnd), (width))
#define ufromfp(x, rnd, width)    __RIN_TGMATH_FROMFP(ufromfp, (x), (rnd), (width))
#define fromfpx(x, rnd, width)    __RIN_TGMATH_FROMFP(fromfpx, (x), (rnd), (width))
#define ufromfpx(x, rnd, width)   __RIN_TGMATH_FROMFP(ufromfpx, (x), (rnd), (width))

/* These functions choose a variant from their floating argument; their
 * other parameters keep the types specified by math.h. */
#define frexp(x, exp) \
    _Generic((x), float: frexpf, long double: frexpl, default: frexp)((x), (exp))
#define modf(x, iptr) \
    _Generic((x), float: modff, long double: modfl, default: modf)((x), (iptr))
#define ldexp(x, exp) \
    _Generic((x), float: ldexpf, long double: ldexpl, default: ldexp)((x), (exp))
#define nexttoward(x, y) \
    _Generic((x), float: nexttowardf, long double: nexttowardl, \
             default: nexttoward)((x), (y))
/* Complex-only functions accept real and complex standard floating inputs. */
#define carg(x) \
    _Generic((x), float: cargf, float _Complex: cargf, \
             long double: cargl, long double _Complex: cargl, default: carg)(x)
#define cimag(x) \
    _Generic((x), float: cimagf, float _Complex: cimagf, \
             long double: cimagl, long double _Complex: cimagl, default: cimag)(x)
#define conj(x) \
    _Generic((x), float: conjf, float _Complex: conjf, \
             long double: conjl, long double _Complex: conjl, default: conj)(x)
#define cproj(x) \
    _Generic((x), float: cprojf, float _Complex: cprojf, \
             long double: cprojl, long double _Complex: cprojl, default: cproj)(x)
#define creal(x) \
    _Generic((x), float: crealf, float _Complex: crealf, \
             long double: creall, long double _Complex: creall, default: creal)(x)

#endif /* !__cplusplus */

#endif /* _TGMATH_H */
