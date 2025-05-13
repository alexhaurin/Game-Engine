#include <stdint.h>
#include <cassert>
#include <cmath>
#include "ozz/base/maths/math_constant.h"
#include "ozz/base/maths/internal/simd_math_config.h"

//#if !defined(OZZ_DISABLE_SSE_NATIVE_OPERATORS)
OZZ_INLINE ozz::math::SimdFloat4 operator+(ozz::math::_SimdFloat4 _a,
    ozz::math::_SimdFloat4 _b) {
    return _mm_add_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _a,
    ozz::math::_SimdFloat4 _b) {
    return _mm_sub_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _v) {
    return _mm_sub_ps(_mm_setzero_ps(), _v);
}

OZZ_INLINE ozz::math::SimdFloat4 operator*(ozz::math::_SimdFloat4 _a,
    ozz::math::_SimdFloat4 _b) {
    return _mm_mul_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator/(ozz::math::_SimdFloat4 _a,
    ozz::math::_SimdFloat4 _b) {
    return _mm_div_ps(_a, _b);
}
//#endif  // !defined(OZZ_DISABLE_SSE_NATIVE_OPERATORS)
