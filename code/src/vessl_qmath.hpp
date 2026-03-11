#pragma once

#include "common/dsp/math/qmath.hpp"
#include "vessicle/vessl/vessl.h"

namespace vessl
{
    // wrapper class for kastle2::q31_t so we can more easily use it as a sample type in vessl classes.
    struct q31_t
    {
        kastle2::q31_t v_;
    
        constexpr q31_t() : v_(kastle2::Q31_ZERO) {}
        constexpr q31_t(const q31_t& copy) : v_(copy.v_) {}
        constexpr q31_t(const kastle2::q31_t& v) : v_(v) {}
        
        explicit constexpr q31_t(const phase_t& v) : v_(kastle2::Q31_ZERO) { *this = v; }
        explicit constexpr q31_t(const analog_t& v) : v_(kastle2::Q31_ZERO) { *this = v; }

        constexpr q31_t& operator=(const q31_t& x) { v_ = x.v_; return *this; }
        constexpr q31_t& operator=(const phase_t& x) { v_ = x/2; return *this; }
        constexpr q31_t& operator=(const analog_t& x) 
        {
            analog_t y = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x); 
            if (y == 1.0f)
            {
                v_ = kastle2::Q31_MAX;
            }
            else if (y == -1.f)
            {
                v_ = kastle2::Q31_MIN;
            }
            else
            {
                v_ = static_cast<kastle2::q31_t>(y * 2147483648.0);
            }
            return *this;
        }

        constexpr operator kastle2::q31_t() const { return v_; }
        // clamps value to [0,1], expands to phase_t range.
        explicit constexpr operator phase_t() const { return static_cast<phase_t>(v_)*2; }
        explicit constexpr operator analog_t() const { return kastle2::q31_to_float(v_); }

        // prefix increment
        q31_t& operator++() { v_ = kastle2::q31_add(v_, 1); return *this; }
        // postfix increment
        q31_t operator++(int) { q31_t p = *this; operator++(); return p; } 
        // prefix decrement
        q31_t& operator--() { v_ = kastle2::q31_sub(v_, 1); return *this; } 
        // postfix decrement
        q31_t operator--(int) { q31_t p = *this; operator--(); return p; }

        q31_t& operator+=(const q31_t& rhs) { v_ = kastle2::q31_add(v_, rhs.v_); return *this; }
        friend q31_t operator+(q31_t lhs, const q31_t& rhs) { lhs += rhs; return lhs; }

        q31_t& operator-=(const q31_t& rhs) { v_ = kastle2::q31_sub(v_, rhs.v_); return *this; }
        friend q31_t operator-(q31_t lhs, const q31_t& rhs) { lhs -= rhs; return lhs; }

        q31_t& operator*=(const q31_t& rhs) { v_ = kastle2::q31_mult(v_, rhs.v_); return *this; }
        friend q31_t operator*(q31_t lhs, const q31_t& rhs) { lhs *= rhs; return lhs; }
    
        q31_t& operator/=(const q31_t& rhs) { v_ = kastle2::q31_div(v_, rhs.v_); return *this; }
        friend q31_t operator/(q31_t lhs, const q31_t& rhs) { lhs /= rhs; return lhs; }
    };

    template<>
    constexpr q31_t cast<q31_t, phase_t>(phase_t from) { return q31_t(from); }

    template<>
    constexpr phase_t cast<phase_t, q31_t>(q31_t from) { return static_cast<phase_t>(from); }

    template<>
    constexpr analog_t cast<analog_t, q31_t>(q31_t from) { return static_cast<analog_t>(from); }

    // float_to_q31 clamps [0,1]
    template<>
    constexpr q31_t cast<q31_t, analog_t>(analog_t from) { return q31_t(from); }

    namespace math
    {
        template<>
        inline q31_t sinz<q31_t>(phase_t p) { return kastle2::q31_sine(cast<q31_t>(p)); }
        
        template<>
        inline q31_t cosz<q31_t>(phase_t p) { return kastle2::q31_sine(cast<q31_t>(p + PHASE_HALF/2)); }

        template<>
        inline analog_t sinz<analog_t>(phase_t p) { return cast<analog_t>(sinz<q31_t>(p)); }
        
        template<>
        inline analog_t cosz<analog_t>(phase_t p) { return cast<analog_t>(sinz<q31_t>(p)); }
    }

    namespace easing
    {
        template<>
        inline q31_t lerpp<q31_t>(q31_t begin, q31_t end, phase_t t) 
        { 
            if (t == PHASE_ZERO) return begin;
            if (t == PHASE_MAX) return end;
            // convert t to fixed-point, do lerp in q31 space.
            q31_t qt = t;
            return begin + (end-begin)*qt;
        }

        template<>
        inline q31_t smooth<q31_t>(q31_t value, q31_t target, analog_t degree) 
        {
            q31_t qd = degree;
            return value*qd + (q31_t(kastle2::Q31_MAX) - qd)*target; 
        }
    }
}