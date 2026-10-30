/* C99 complex elementary functions. */
#include "complex.h"
#include <math.h>
#include "fenv.h"

static double complex cx_make(double real, double imaginary) {
    return CMPLX(real, imaginary);
}

static double cx_real(double complex value) { return __real__ value; }
static double cx_imag(double complex value) { return __imag__ value; }

static void cx_raise(int excepts) {
    (void)feraiseexcept(excepts);
}

void __rin_cmul_f64(double, double, double, double, double*, double*);
void __rin_cmul_f32(float, float, float, float, float*, float*);

static double cx_acosh_nonnegative(double value) {
    if (value == 1.0) return 0.0;
    if (value > 1.0e150) return log(value) + M_LN2;
    return acosh(value);
}

static void cx_ellipse(double x, double y, double* scale,
                       double* alpha_scaled) {
    double magnitude = fmax(fabs(x), fabs(y));
    double reciprocal;
    double scaled_x, scaled_y;
    double radius_plus, radius_minus;
    *scale = magnitude > 1.0 ? magnitude : 1.0;
    reciprocal = 1.0 / *scale;
    scaled_x = x / *scale;
    scaled_y = y / *scale;
    radius_plus = hypot(scaled_x + reciprocal, scaled_y);
    radius_minus = hypot(scaled_x - reciprocal, scaled_y);
    *alpha_scaled = 0.5 * radius_plus + 0.5 * radius_minus;
}

static double cx_acosh_scaled(double scale, double alpha_scaled) {
    if (scale > 1.0e150 / alpha_scaled)
        return log(scale) + log(alpha_scaled) + M_LN2;
    return cx_acosh_nonnegative(scale * alpha_scaled);
}

static double complex cx_mul(double complex lhs, double complex rhs) {
    double a = cx_real(lhs), b = cx_imag(lhs);
    double c = cx_real(rhs), d = cx_imag(rhs);
    double real, imaginary;
    __rin_cmul_f64(a, b, c, d, &real, &imaginary);
    return cx_make(real, imaginary);
}

void __rin_cmul_f64(double a, double b, double c, double d,
                    double* real, double* imaginary) {
    double ac = a * c, bd = b * d, ad = a * d, bc = b * c;
    double x = ac - bd;
    double y = ad + bc;
    if (isnan(x) && isnan(y)) {
        int recover = 0;
        if (isinf(a) || isinf(b)) {
            a = copysign(isinf(a) ? 1.0 : 0.0, a);
            b = copysign(isinf(b) ? 1.0 : 0.0, b);
            if (isnan(c)) c = copysign(0.0, c);
            if (isnan(d)) d = copysign(0.0, d);
            recover = 1;
        }
        if (isinf(c) || isinf(d)) {
            c = copysign(isinf(c) ? 1.0 : 0.0, c);
            d = copysign(isinf(d) ? 1.0 : 0.0, d);
            if (isnan(a)) a = copysign(0.0, a);
            if (isnan(b)) b = copysign(0.0, b);
            recover = 1;
        }
        if (!recover && (isinf(ac) || isinf(bd) || isinf(ad) || isinf(bc))) {
            if (isnan(a)) a = copysign(0.0, a);
            if (isnan(b)) b = copysign(0.0, b);
            if (isnan(c)) c = copysign(0.0, c);
            if (isnan(d)) d = copysign(0.0, d);
            recover = 1;
        }
        if (recover) {
            x = INFINITY * (a * c - b * d);
            y = INFINITY * (a * d + b * c);
        }
    }
    *real = x;
    *imaginary = y;
}

void __rin_cmul_f32(float a, float b, float c, float d,
                    float* real, float* imaginary) {
    float ac = a * c, bd = b * d, ad = a * d, bc = b * c;
    float x = ac - bd;
    float y = ad + bc;
    if (isnan(x) && isnan(y)) {
        int recover = 0;
        if (isinf(a) || isinf(b)) {
            a = copysignf(isinf(a) ? 1.0f : 0.0f, a);
            b = copysignf(isinf(b) ? 1.0f : 0.0f, b);
            if (isnan(c)) c = copysignf(0.0f, c);
            if (isnan(d)) d = copysignf(0.0f, d);
            recover = 1;
        }
        if (isinf(c) || isinf(d)) {
            c = copysignf(isinf(c) ? 1.0f : 0.0f, c);
            d = copysignf(isinf(d) ? 1.0f : 0.0f, d);
            if (isnan(a)) a = copysignf(0.0f, a);
            if (isnan(b)) b = copysignf(0.0f, b);
            recover = 1;
        }
        if (!recover && (isinf(ac) || isinf(bd) || isinf(ad) || isinf(bc))) {
            if (isnan(a)) a = copysignf(0.0f, a);
            if (isnan(b)) b = copysignf(0.0f, b);
            if (isnan(c)) c = copysignf(0.0f, c);
            if (isnan(d)) d = copysignf(0.0f, d);
            recover = 1;
        }
        if (recover) {
            x = HUGE_VALF * (a * c - b * d);
            y = HUGE_VALF * (a * d + b * c);
        }
    }
    *real = x;
    *imaginary = y;
}

void __rin_cdiv_f64(double a, double b, double c, double d,
                    double* real, double* imaginary) {
    if (c == 0.0 && d == 0.0) {
        cx_raise(FE_DIVBYZERO);
        if (a == 0.0 && b == 0.0) cx_raise(FE_INVALID);
        double scale = copysign(INFINITY, c);
        *real = scale * a;
        *imaginary = scale * b;
        return;
    }
    if ((isinf(c) || isinf(d)) && isfinite(a) && isfinite(b)) {
        double cn = copysign(isinf(c) ? 1.0 : 0.0, c);
        double dn = copysign(isinf(d) ? 1.0 : 0.0, d);
        *real = 0.0 * (a * cn + b * dn);
        *imaginary = 0.0 * (b * cn - a * dn);
        return;
    }
    if ((isinf(a) || isinf(b)) && isfinite(c) && isfinite(d)) {
        double an = copysign(isinf(a) ? 1.0 : 0.0, a);
        double bn = copysign(isinf(b) ? 1.0 : 0.0, b);
        *real = INFINITY * (an * c + bn * d);
        *imaginary = INFINITY * (bn * c - an * d);
        return;
    }
    if (isfinite(a) && isfinite(b) && isfinite(c) && isfinite(d)) {
        double numerator_scale = fabs(a) >= fabs(b) ? fabs(a) : fabs(b);
        double denominator_scale = fabs(c) >= fabs(d) ? fabs(c) : fabs(d);
        double an, bn, cn = c / denominator_scale;
        double dn = d / denominator_scale;
        double denominator = cn * cn + dn * dn;
        double real_unit, imaginary_unit;
        int numerator_exponent, denominator_exponent;
        double numerator_mantissa, denominator_mantissa;
        if (numerator_scale == 0.0) {
            an = a;
            bn = b;
            *real = copysign(0.0, (an * cn + bn * dn) / denominator);
            *imaginary = copysign(
                0.0, (bn * cn - an * dn) / denominator);
            return;
        }
        an = a / numerator_scale;
        bn = b / numerator_scale;
        real_unit = (an * cn + bn * dn) / denominator;
        imaginary_unit = (bn * cn - an * dn) / denominator;
        numerator_mantissa = frexp(numerator_scale, &numerator_exponent);
        denominator_mantissa = frexp(
            denominator_scale, &denominator_exponent);
        *real = ldexp(real_unit * (numerator_mantissa /
                                   denominator_mantissa),
                      numerator_exponent - denominator_exponent);
        *imaginary = ldexp(imaginary_unit * (numerator_mantissa /
                                             denominator_mantissa),
                           numerator_exponent - denominator_exponent);
        return;
    }
    *real = (a * c + b * d) / (c * c + d * d);
    *imaginary = (b * c - a * d) / (c * c + d * d);
}

void __rin_cdiv_f32(float a, float b, float c, float d,
                    float* real, float* imaginary) {
    if (c == 0.0f && d == 0.0f) {
        cx_raise(FE_DIVBYZERO);
        if (a == 0.0f && b == 0.0f) cx_raise(FE_INVALID);
        float scale = copysignf(HUGE_VALF, c);
        *real = scale * a;
        *imaginary = scale * b;
        return;
    }
    if ((isinf(c) || isinf(d)) && isfinite(a) && isfinite(b)) {
        float cn = copysignf(isinf(c) ? 1.0f : 0.0f, c);
        float dn = copysignf(isinf(d) ? 1.0f : 0.0f, d);
        *real = 0.0f * (a * cn + b * dn);
        *imaginary = 0.0f * (b * cn - a * dn);
        return;
    }
    if ((isinf(a) || isinf(b)) && isfinite(c) && isfinite(d)) {
        float an = copysignf(isinf(a) ? 1.0f : 0.0f, a);
        float bn = copysignf(isinf(b) ? 1.0f : 0.0f, b);
        *real = HUGE_VALF * (an * c + bn * d);
        *imaginary = HUGE_VALF * (bn * c - an * d);
        return;
    }
    if (isfinite(a) && isfinite(b) && isfinite(c) && isfinite(d)) {
        float numerator_scale = fabsf(a) >= fabsf(b) ? fabsf(a) : fabsf(b);
        float denominator_scale = fabsf(c) >= fabsf(d) ? fabsf(c) : fabsf(d);
        float an, bn, cn = c / denominator_scale;
        float dn = d / denominator_scale;
        float denominator = cn * cn + dn * dn;
        float real_unit, imaginary_unit;
        int numerator_exponent, denominator_exponent;
        float numerator_mantissa, denominator_mantissa;
        if (numerator_scale == 0.0f) {
            an = a;
            bn = b;
            *real = copysignf(0.0f, (an * cn + bn * dn) / denominator);
            *imaginary = copysignf(
                0.0f, (bn * cn - an * dn) / denominator);
            return;
        }
        an = a / numerator_scale;
        bn = b / numerator_scale;
        real_unit = (an * cn + bn * dn) / denominator;
        imaginary_unit = (bn * cn - an * dn) / denominator;
        numerator_mantissa = frexpf(numerator_scale, &numerator_exponent);
        denominator_mantissa = frexpf(
            denominator_scale, &denominator_exponent);
        *real = ldexpf(real_unit * (numerator_mantissa /
                                    denominator_mantissa),
                       numerator_exponent - denominator_exponent);
        *imaginary = ldexpf(imaginary_unit * (numerator_mantissa /
                                              denominator_mantissa),
                            numerator_exponent - denominator_exponent);
        return;
    }
    *real = (a * c + b * d) / (c * c + d * d);
    *imaginary = (b * c - a * d) / (c * c + d * d);
}

double creal(double complex value) { return cx_real(value); }
double cimag(double complex value) { return cx_imag(value); }
float crealf(float complex value) { return __real__ value; }
float cimagf(float complex value) { return __imag__ value; }

double cabs(double complex value) {
    return hypot(cx_real(value), cx_imag(value));
}

float cabsf(float complex value) {
    return hypotf(__real__ value, __imag__ value);
}

double carg(double complex value) {
    return atan2(cx_imag(value), cx_real(value));
}

float cargf(float complex value) {
    return atan2f(__imag__ value, __real__ value);
}

double complex conj(double complex value) {
    return cx_make(cx_real(value), -cx_imag(value));
}

float complex conjf(float complex value) {
    return CMPLXF(__real__ value, -__imag__ value);
}

double complex cproj(double complex value) {
    if (isinf(cx_real(value)) || isinf(cx_imag(value)))
        return cx_make(INFINITY, copysign(0.0, cx_imag(value)));
    return value;
}

float complex cprojf(float complex value) {
    if (isinf(__real__ value) || isinf(__imag__ value))
        return CMPLXF(INFINITY, copysignf(0.0f, __imag__ value));
    return value;
}

double complex cexp(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    double scale;
    if (isinf(y)) {
        if (isinf(x) && x < 0.0) return cx_make(0.0, 0.0);
        if (isinf(x) && x > 0.0) {
            cx_raise(FE_INVALID);
            return cx_make(INFINITY, NAN);
        }
        cx_raise(FE_INVALID);
        return cx_make(NAN, NAN);
    }
    if (isnan(y)) {
        if (isinf(x) && x < 0.0) return cx_make(0.0, 0.0);
        if (isinf(x) && x > 0.0) return cx_make(INFINITY, NAN);
        return cx_make(NAN, NAN);
    }
    if (isnan(x)) {
        if (y == 0.0) return cx_make(NAN, y);
        return cx_make(NAN, NAN);
    }
    if (isinf(x)) {
        if (x < 0.0) return cx_make(0.0 * cos(y), 0.0 * sin(y));
        if (y == 0.0) return cx_make(INFINITY, copysign(0.0, y));
        return cx_make(INFINITY * cos(y), INFINITY * sin(y));
    }
    scale = exp(x);
    if (y == 0.0) return cx_make(scale, copysign(0.0, y));
    return cx_make(scale * cos(y), scale * sin(y));
}

double complex clog(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(x) || isinf(y)) {
        if (isnan(x) || isnan(y)) return cx_make(INFINITY, NAN);
        return cx_make(INFINITY, atan2(y, x));
    }
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    if (x == 0.0 && y == 0.0) {
        cx_raise(FE_DIVBYZERO);
        return cx_make(-INFINITY, atan2(y, x));
    }
    {
        double absolute_x = fabs(x), absolute_y = fabs(y);
        double scale = absolute_x >= absolute_y ? absolute_x : absolute_y;
        double ratio = absolute_x >= absolute_y
            ? absolute_y / scale : absolute_x / scale;
        return cx_make(log(scale) + 0.5 * log1p(ratio * ratio),
                       atan2(y, x));
    }
}

double complex csqrt(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    double radius, real, imaginary;
    if (isinf(y))
        return cx_make(INFINITY, copysign(INFINITY, y));
    if (isinf(x)) {
        if (x > 0.0) {
            if (isnan(y)) return cx_make(INFINITY, NAN);
            return cx_make(INFINITY, copysign(0.0, y));
        }
        if (isnan(y)) return cx_make(NAN, copysign(INFINITY, y));
        return cx_make(0.0, copysign(INFINITY, y));
    }
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    if (x == 0.0 && y == 0.0)
        return cx_make(0.0, y);
    {
        double scale = fmax(fabs(x), fabs(y));
        double scaled_x = x / scale;
        double scaled_y = y / scale;
        radius = hypot(scaled_x, scaled_y);
        if (x >= 0.0) {
            real = sqrt(scale) * sqrt(0.5 * radius + 0.5 * scaled_x);
            imaginary = y / (2.0 * real);
        } else {
            imaginary = copysign(
                sqrt(scale) * sqrt(0.5 * radius - 0.5 * scaled_x), y);
            real = fabs(y) / (2.0 * fabs(imaginary));
        }
    }
    return cx_make(real, imaginary);
}

double complex cpow(double complex base, double complex exponent) {
    double complex logarithm;
    double exponent_real = cx_real(exponent);
    double exponent_imaginary = cx_imag(exponent);
    double base_real = cx_real(base);
    double base_imaginary = cx_imag(base);
    if (exponent_real == 0.0 && exponent_imaginary == 0.0)
        return cx_make(1.0, 0.0);
    if (base_real == 1.0 && base_imaginary == 0.0)
        return cx_make(1.0, 0.0);
    if (base_real == 0.0 && base_imaginary == 0.0) {
        if (exponent_real > 0.0)
            return cx_make(0.0, copysign(0.0, base_imaginary));
        if (exponent_real < 0.0) {
            cx_raise(FE_DIVBYZERO);
            return cx_make(INFINITY, copysign(0.0, base_imaginary));
        }
        return cx_make(NAN, NAN);
    }
    logarithm = clog(base);
    return cexp(cx_mul(exponent, logarithm));
}

double complex csinh(double complex value);
double complex ccosh(double complex value);
double complex ctanh(double complex value);

double complex csin(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    double complex rotated = cx_make(-y, x);
    double complex result = csinh(rotated);
    return cx_make(cx_imag(result), -cx_real(result));
}

double complex ccos(double complex value) {
    return ccosh(cx_make(-cx_imag(value), cx_real(value)));
}

double complex ctan(double complex value) {
    double complex result = ctanh(cx_make(-cx_imag(value), cx_real(value)));
    return cx_make(cx_imag(result), -cx_real(result));
}

double complex csinh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(x)) {
        if (isnan(y)) return cx_make(copysign(INFINITY, x), NAN);
        if (isinf(y)) {
            cx_raise(FE_INVALID);
            return cx_make(copysign(INFINITY, x), NAN);
        }
        if (y == 0.0) return cx_make(x, y);
        return cx_make(sinh(x) * cos(y), cosh(x) * sin(y));
    }
    if (isinf(y)) {
        if (x == 0.0) {
            cx_raise(FE_INVALID);
            return cx_make(copysign(0.0, x), NAN);
        }
        if (isfinite(x)) {
            cx_raise(FE_INVALID);
            return cx_make(NAN, NAN);
        }
        return cx_make(NAN, NAN);
    }
    if (isnan(y)) {
        if (x == 0.0) return cx_make(copysign(0.0, x), NAN);
        if (isinf(x)) return cx_make(copysign(INFINITY, x), NAN);
        return cx_make(NAN, NAN);
    }
    if (isnan(x)) {
        if (y == 0.0) return cx_make(NAN, y);
        return cx_make(NAN, NAN);
    }
    return cx_make(sinh(x) * cos(y), cosh(x) * sin(y));
}

double complex ccosh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(x)) {
        if (isnan(y)) return cx_make(INFINITY, NAN);
        if (isinf(y)) {
            cx_raise(FE_INVALID);
            return cx_make(INFINITY, NAN);
        }
        if (y == 0.0) return cx_make(INFINITY, copysign(0.0, y));
        return cx_make(cosh(x) * cos(y), sinh(x) * sin(y));
    }
    if (isinf(y)) {
        if (x == 0.0) {
            cx_raise(FE_INVALID);
            return cx_make(NAN, copysign(0.0, y));
        }
        if (isfinite(x)) {
            cx_raise(FE_INVALID);
            return cx_make(NAN, NAN);
        }
        return cx_make(NAN, NAN);
    }
    if (isnan(y)) {
        if (x == 0.0) return cx_make(NAN, copysign(0.0, y));
        if (isinf(x)) return cx_make(INFINITY, NAN);
        return cx_make(NAN, NAN);
    }
    if (isnan(x)) {
        if (y == 0.0) return cx_make(NAN, copysign(0.0, y));
        return cx_make(NAN, NAN);
    }
    return cx_make(cosh(x) * cos(y), sinh(x) * sin(y));
}

double complex ctanh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(x)) {
        if (isnan(y) || isinf(y)) return cx_make(copysign(1.0, x), 0.0);
        return cx_make(copysign(1.0, x), copysign(0.0, sin(2.0 * y)));
    }
    if (isinf(y)) {
        cx_raise(FE_INVALID);
        if (x == 0.0) return cx_make(x, NAN);
        return cx_make(NAN, NAN);
    }
    if (isnan(y)) {
        if (x == 0.0) return cx_make(x, y);
        return cx_make(NAN, NAN);
    }
    if (isnan(x)) {
        if (y == 0.0) return cx_make(NAN, y);
        return cx_make(NAN, NAN);
    }
    if (y == 0.0) return cx_make(tanh(x), y);
    if (fabs(x) > 20.0) {
        double scaled = exp(-2.0 * fabs(x));
        double denominator = 1.0 + scaled * scaled +
                             2.0 * scaled * cos(2.0 * y);
        return cx_make(copysign((1.0 - scaled * scaled) / denominator, x),
                       2.0 * scaled * sin(2.0 * y) / denominator);
    }
    double denominator = cosh(2.0 * x) + cos(2.0 * y);
    return cx_make(sinh(2.0 * x) / denominator,
                   sin(2.0 * y) / denominator);
}

double complex casin(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    double scale, alpha_scaled;
    if (isinf(x) || isinf(y)) {
        if (isinf(x) && isinf(y))
            return cx_make(copysign(M_PI_4, x), copysign(INFINITY, y));
        if (isinf(x) && isnan(y))
            return cx_make(NAN, copysign(INFINITY, y));
        if (isinf(x))
            return cx_make(copysign(M_PI_2, x), copysign(INFINITY, y));
        if (isnan(x)) return cx_make(NAN, copysign(INFINITY, y));
        return cx_make(copysign(0.0, x), copysign(INFINITY, y));
    }
    if (isnan(x) && y == 0.0) return cx_make(NAN, y);
    if (x == 0.0 && isnan(y)) return cx_make(x, NAN);
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    cx_ellipse(x, y, &scale, &alpha_scaled);
    double beta = (x / scale) / alpha_scaled;
    if (beta > 1.0) beta = 1.0;
    if (beta < -1.0) beta = -1.0;
    double real = asin(beta);
    double imaginary = copysign(cx_acosh_scaled(scale, alpha_scaled), y);
    return cx_make(real, imaginary);
}

double complex cacos(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(y)) {
        if (isinf(x))
            return cx_make(x < 0.0 ? 3.0 * M_PI_4 : M_PI_4,
                           copysign(INFINITY, -y));
        if (isnan(x)) return cx_make(NAN, copysign(INFINITY, -y));
        return cx_make(M_PI_2, copysign(INFINITY, -y));
    }
    if (isinf(x)) {
        if (isnan(y)) return cx_make(NAN, -INFINITY);
        return cx_make(x < 0.0 ? M_PI : 0.0,
                       copysign(INFINITY, -y));
    }
    if (isnan(y)) {
        if (x == 0.0) return cx_make(M_PI_2, NAN);
        return cx_make(NAN, NAN);
    }
    if (isnan(x)) return cx_make(NAN, NAN);
    double complex inverse_sine = casin(value);
    return cx_make(M_PI_2 - cx_real(inverse_sine), -cx_imag(inverse_sine));
}

double complex catan(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    double absolute_x = fabs(x), absolute_y = fabs(y);
    double scale = absolute_x >= absolute_y ? absolute_x : absolute_y;
    double real, ratio, imaginary;
    if (isnan(x) && y == 0.0) return cx_make(NAN, y);
    if (isnan(x) && isinf(y)) return cx_make(NAN, copysign(0.0, y));
    if (isinf(x) || isinf(y))
        return cx_make(copysign(M_PI_2, x), copysign(0.0, y));
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    if (x == 0.0 && (y == 1.0 || y == -1.0))
        cx_raise(FE_DIVBYZERO);
    if (scale > 1.0) {
        double inverse = 1.0 / scale;
        double scaled_x = x / scale;
        double scaled_y = y / scale;
        real = 0.5 * atan2(2.0 * scaled_x * inverse,
                           inverse * inverse - scaled_x * scaled_x -
                               scaled_y * scaled_y);
    } else {
        real = 0.5 * atan2(2.0 * x, 1.0 - x * x - y * y);
    }
    if (scale > 1.0) {
        double inverse = 1.0 / scale;
        double scaled_x = x / scale;
        double scaled_y = (y - 1.0) / scale;
        double denominator = scaled_x * scaled_x + scaled_y * scaled_y;
        ratio = 4.0 * (y / scale) * inverse / denominator;
    } else {
        double denominator = x * x + (y - 1.0) * (y - 1.0);
        ratio = denominator == 0.0
            ? INFINITY : (4.0 * y) / denominator;
    }
    imaginary = 0.25 * log1p(ratio);
    return cx_make(real, imaginary);
}

double complex casinh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(x)) {
        if (isnan(y)) return cx_make(x, NAN);
        if (isinf(y))
            return cx_make(copysign(INFINITY, x), copysign(M_PI_4, y));
        return cx_make(x, copysign(0.0, y));
    }
    if (isnan(x) && isinf(y)) return cx_make(INFINITY, NAN);
    if (isinf(y))
        return cx_make(copysign(INFINITY, x), copysign(M_PI_2, y));
    if (isnan(x) && y == 0.0) return cx_make(NAN, y);
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    double complex rotated = cx_make(-cx_imag(value), cx_real(value));
    double complex result = casin(rotated);
    return cx_make(cx_imag(result), -cx_real(result));
}

double complex cacosh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (isinf(y)) {
        if (isinf(x))
            return cx_make(INFINITY, copysign(x < 0.0 ? 3.0 * M_PI_4 : M_PI_4, y));
        if (isnan(x)) return cx_make(INFINITY, NAN);
        return cx_make(INFINITY, copysign(M_PI_2, y));
    }
    if (isinf(x)) {
        if (isnan(y)) return cx_make(INFINITY, NAN);
        if (x > 0.0) return cx_make(INFINITY, copysign(0.0, y));
        return cx_make(INFINITY, copysign(M_PI, y));
    }
    if (x == 0.0 && isnan(y)) return cx_make(NAN, M_PI_2);
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    double scale, alpha_scaled;
    cx_ellipse(x, y, &scale, &alpha_scaled);
    double real = cx_acosh_scaled(scale, alpha_scaled);
    double ratio = (x / scale) / alpha_scaled;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < -1.0) ratio = -1.0;
    return cx_make(real, copysign(acos(ratio), y));
}

double complex catanh(double complex value) {
    double x = cx_real(value), y = cx_imag(value);
    if (x == 0.0 && y == 0.0) return cx_make(x, y);
    if (isinf(x) && isnan(y)) return cx_make(copysign(0.0, x), NAN);
    if (isinf(x) || isinf(y)) {
        return cx_make(copysign(0.0, x), copysign(M_PI_2, y));
    }
    if (x == 0.0 && isnan(y)) return cx_make(x, NAN);
    if (isnan(x) || isnan(y)) return cx_make(NAN, NAN);
    if ((x == 1.0 || x == -1.0) && y == 0.0)
        cx_raise(FE_DIVBYZERO);
    double complex iz = cx_make(-cx_imag(value), cx_real(value));
    double complex result = catan(iz);
    return cx_make(cx_imag(result), -cx_real(result));
}

/* Narrowing wrappers are deliberately expressed through the double kernels;
 * results are rounded once at the public float return boundary. */
#define CX_FLOAT_UNARY(name) \
    float complex name##f(float complex value) { \
        double complex wide = CMPLX((double)__real__ value, \
                                    (double)__imag__ value); \
        double complex result = name(wide); \
        return CMPLXF(cx_real(result), cx_imag(result)); \
    }

CX_FLOAT_UNARY(cexp)
CX_FLOAT_UNARY(clog)
CX_FLOAT_UNARY(csqrt)
CX_FLOAT_UNARY(csin)
CX_FLOAT_UNARY(ccos)
CX_FLOAT_UNARY(ctan)
CX_FLOAT_UNARY(csinh)
CX_FLOAT_UNARY(ccosh)
CX_FLOAT_UNARY(ctanh)
CX_FLOAT_UNARY(casin)
CX_FLOAT_UNARY(cacos)
CX_FLOAT_UNARY(catan)
CX_FLOAT_UNARY(casinh)
CX_FLOAT_UNARY(cacosh)
CX_FLOAT_UNARY(catanh)

float complex cpowf(float complex base, float complex exponent) {
    double complex result = cpow(
        CMPLX(__real__ base, __imag__ base),
        CMPLX(__real__ exponent, __imag__ exponent));
    return CMPLXF(cx_real(result), cx_imag(result));
}

long double creall(long double complex value) { return __real__ value; }
long double cimagl(long double complex value) { return __imag__ value; }

long double cabsl(long double complex value) {
    return (long double)hypot((double)__real__ value, (double)__imag__ value);
}

long double cargl(long double complex value) {
    return (long double)atan2((double)__imag__ value, (double)__real__ value);
}

long double complex conjl(long double complex value) {
    return CMPLXL(__real__ value, -__imag__ value);
}

long double complex cprojl(long double complex value) {
    if (isinf((double)__real__ value) || isinf((double)__imag__ value))
        return CMPLXL(INFINITY, copysign(0.0, (double)__imag__ value));
    return value;
}

#define CX_LONG_UNARY(name) \
    long double complex name##l(long double complex value) { \
        double complex wide = CMPLX((double)__real__ value, \
                                    (double)__imag__ value); \
        double complex result = name(wide); \
        return CMPLXL(cx_real(result), cx_imag(result)); \
    }

CX_LONG_UNARY(cexp)
CX_LONG_UNARY(clog)
CX_LONG_UNARY(csqrt)
CX_LONG_UNARY(csin)
CX_LONG_UNARY(ccos)
CX_LONG_UNARY(ctan)
CX_LONG_UNARY(csinh)
CX_LONG_UNARY(ccosh)
CX_LONG_UNARY(ctanh)
CX_LONG_UNARY(casin)
CX_LONG_UNARY(cacos)
CX_LONG_UNARY(catan)
CX_LONG_UNARY(casinh)
CX_LONG_UNARY(cacosh)
CX_LONG_UNARY(catanh)

long double complex cpowl(long double complex base,
                          long double complex exponent) {
    double complex result = cpow(
        CMPLX((double)__real__ base, (double)__imag__ base),
        CMPLX((double)__real__ exponent, (double)__imag__ exponent));
    return CMPLXL(cx_real(result), cx_imag(result));
}
