/*
MIT License

Copyright (c) 2026 Damien Quartz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include "vessicle/vessl/vessl.h"
#include "common/dsp/math/qmath.hpp"

namespace quartz_kastle
{
// App Ids for all quartz_kastle Apps
namespace app_id
{
  static constexpr uint8_t kKnoscillator = 0x3D;
}
}

namespace vessl
{
using k31_t = kastle2::q31_t;
using v31_t = q31;

template<>
VESSL_INLINE constexpr k31_t cast<k31_t, v31_t>(const v31_t& from) { return from.v_; } 

template<>
VESSL_INLINE constexpr k31_t cast<k31_t, phase_t>(const phase_t& from) { return cast<v31_t>(from).v_; }

template<>
VESSL_INLINE constexpr phase_t cast<phase_t, k31_t>(const k31_t& from) { return cast<phase_t>(v31_t(from)); }

template<>
VESSL_INLINE constexpr analog_t cast<analog_t, k31_t>(const k31_t& from) { return kastle2::q31_to_float(from); }

// float_to_q31 clamps [0,1]
template<>
VESSL_INLINE constexpr k31_t cast<k31_t, analog_t>(const analog_t& from) { return kastle2::float_to_q31(from); }

namespace math
{
template<>
VESSL_INLINE k31_t abs<k31_t>(const k31_t& v) { return kastle2::q31_abs(v); }

template<>
VESSL_INLINE k31_t sin<k31_t, phase_t>(phase_t p) { return kastle2::q31_sine(p >> 1); }

template<>
VESSL_INLINE k31_t cos<k31_t, phase_t>(phase_t p) { return sin<k31_t>(p + phase_90); }

template<>
VESSL_INLINE k31_t lerp<k31_t>(k31_t begin, k31_t end, phase_t t) 
{ 
    if (t == phase_zero) return begin;
    if (t == phase_360) return end;
    // convert t to q31, do lerp in q31 space.
    k31_t qt = cast<k31_t>(t);
    k31_t delta = kastle2::q31_mult(kastle2::q31_sub(end, begin), qt);
    return kastle2::q31_add(begin, delta);
}

namespace easing
{

template<>
VESSL_INLINE k31_t smooth<k31_t>(k31_t value, k31_t target, analog_t degree) 
{
    k31_t qd = kastle2::float_to_q31(degree);
    return value*qd + (kastle2::Q31_MAX - qd)*target;
}
} // namespace easing
} // namespace math
} // namespace vessl