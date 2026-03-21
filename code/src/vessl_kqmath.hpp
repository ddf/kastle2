#pragma once

#include "common/dsp/math/qmath.hpp"
#include "vessicle/vessl/vessl.h"

namespace vessl
{
    using k31_t = kastle2::q31_t;
    using v31_t = q31;

    template<>
    inline constexpr k31_t cast<k31_t, v31_t>(const v31_t& from) { return from.v_; } 

    template<>
    inline constexpr k31_t cast<k31_t, phase_t>(const phase_t& from) { return cast<v31_t>(from).v_; }

    template<>
    inline constexpr phase_t cast<phase_t, k31_t>(const k31_t& from) { return cast<phase_t>(v31_t(from)); }

    template<>
    inline constexpr analog_t cast<analog_t, k31_t>(const k31_t& from) { return kastle2::q31_to_float(from); }

    // float_to_q31 clamps [0,1]
    template<>
    inline constexpr k31_t cast<k31_t, analog_t>(const analog_t& from) { return kastle2::float_to_q31(from); }

    namespace math
    {
        template<>
        inline k31_t abs<k31_t>(const k31_t& v) { return kastle2::q31_abs(v); }
        
        template<>
        inline k31_t sin<k31_t, phase_t>(phase_t p) { return kastle2::q31_sine(p >> 1); }
        
        template<>
        inline k31_t cos<k31_t, phase_t>(phase_t p) { return sin<k31_t>(p + PHASE_90); }
    }

    namespace easing
    {
        template<>
        inline k31_t lerpp<k31_t>(k31_t begin, k31_t end, phase_t t) 
        { 
            if (t == PHASE_ZERO) return begin;
            if (t == PHASE_360) return end;
            // convert t to q31, do lerp in q31 space.
            k31_t qt = cast<k31_t>(t);
            k31_t delta = kastle2::q31_mult(kastle2::q31_sub(end, begin), qt);
            return kastle2::q31_add(begin, delta);
        }

        template<>
        inline k31_t smooth<k31_t>(k31_t value, k31_t target, analog_t degree) 
        {
            k31_t qd = kastle2::float_to_q31(degree);
            return value*qd + (k31_t(kastle2::Q31_MAX) - qd)*target;
        }
    }
}