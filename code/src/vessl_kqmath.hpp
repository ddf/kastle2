#pragma once

#include "common/dsp/math/qmath.hpp"
#include "vessicle/vessl/vessl.h"

namespace vessl
{
    using q31_t = kastle2::q31_t;

    template<>
    constexpr q31_t cast<q31_t, phase_t>(phase_t from) { return from.v_; }

    template<>
    constexpr phase_t cast<phase_t, q31_t>(q31_t from) { return phase_t(from); }

    template<>
    constexpr analog_t cast<analog_t, q31_t>(q31_t from) { return kastle2::q31_to_float(from); }

    // float_to_q31 clamps [0,1]
    template<>
    constexpr q31_t cast<q31_t, analog_t>(analog_t from) { return kastle2::float_to_q31(from); }

    namespace math
    {
        template<>
        inline q31_t sin<q31_t, phase_t>(phase_t p) { return kastle2::q31_sine(wrap01(p).v_); }
        
        template<>
        inline q31_t cos<q31_t, phase_t>(phase_t p) { return kastle2::q31_sine(wrap01(p.spill(PHASE_90)).v_); }
    }

    namespace easing
    {
        template<>
        inline q31_t lerpp<q31_t>(q31_t begin, q31_t end, phase_t t) 
        { 
            if (t == PHASE_ZERO) return begin;
            if (t == PHASE_360) return end;
            // convert t to q31, do lerp in q31 space.
            q31_t delta = kastle2::q31_mult(kastle2::q31_sub(end, begin), t.v_);
            return kastle2::q31_add(begin, delta);
        }

        template<>
        inline q31_t smooth<q31_t>(q31_t value, q31_t target, analog_t degree) 
        {
            q31_t qd = kastle2::float_to_q31(degree);
            // @todo look at OWL SmoothInt
            return value*qd + (q31_t(kastle2::Q31_MAX) - qd)*target;
        }
    }
}