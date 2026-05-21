/*
MIT License

Copyright (c) 2024 Marek Mach (Bastl Instruments)
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

#include <cstddef>
#include <cstdint>

#include "vessicle/Knoscillator.h"
#include "vessicle/Projector.h"

#include "common/core/App.hpp"
#include "common/core/Hardware.hpp"
#include "common/core/Kastle2.hpp"
#include "common/dsp/control/AdsrEnv.hpp"
#include "common/dsp/utility/Quantizer.hpp"
#include "common/dsp/effects/StereoDelay.hpp"

#include "common/EnumTools.hpp"
#include "common/controls/FancyMode.hpp"
#include "common/controls/FancyPot.hpp"

// not only do we not have the processing power to smooth P&Q,
// trying to use that code overflows the FASTCODE section of the binary.
using Knoscil = Knoscillator<vessl::q31, false>;
using Camera  = Projector<vessl::q31>;

struct KnotDebug 
{
    vessl::phase_t pp, pq, pz;
    kastle2::q31_t kx, ky, kz;
    vessl::phase_t rf, ry;
    int32_t     rr;
    vessl::analog_t ryf;
    vessl::analog_t ryfa;
    kastle2::q31_t proj;
    vessl::phase_t cx, cy, cz;
    kastle2::q31_t left, right;
};

extern KnotDebug gDbg;

using namespace kastle2;

namespace knoscillator
{

class AppKnoscillator : public virtual App
{
public:
    /**
     * @brief Some example modes.
     */
    enum class Mode
    {
        TFOIL_LISSA,
        LISSA_TORUS,
        TORUS_TFOIL,
        COUNT
    };

    /**
     * @brief Initializes all the parameters, memory, etc.
     */
    void Init() override;

    /**
     * @brief Deinitializes the app, stops all effects, etc.
     */
    void DeInit() override;

    /**
     * @brief Called each interrupt loop. Implements all the audio processing.
     * @param input Input buffer.
     * @param output Output buffer.
     * @param size Number of sample pairs in the buffer (real size of the buffer is 2*size).
     */
    FASTCODE void AudioLoop(q15_t *input, q15_t *output, size_t size) override;

    /**
     * @brief Called each time AudioLoop isn't busy.
     */
    void UiLoop() override;

    /**
     * @brief Called only ONCE when the app is started.
     */
    FASTCODE void SecondCoreWorker();

    /**
     * @brief Called when the app is first loaded - initializes the memory values.
     */
    void MemoryInitialization() override;

    /**
     * @brief Handles incoming MIDI messages.
     * @param msg The MIDI message to handle.
     */
    void MidiCallback(midi::Message *msg) override;

    /**
     * @brief Returns the app ID.
     * @return The app ID.
     */
    uint8_t GetId() override
    {
        return Kastle2::kDefaultAppId;
    }

private:
    /**
     * @brief Here the second core generates a buffer of knot audio. 
     * It is called by the SecondCoreWorker.
     */
    FASTCODE void SecondCoreGenerate(size_t index);

    /**
     * @brief Handle trigger events
     *
     * Processes trigger input by storing current pitch CV values,
     * updating mode selector, and triggering the envelope generator.
     */
    void Trigger();

    /**
     * @brief Update delay time based on current clock tempo
     *
     * Calculates delay time based on the average clock ticks and applies
     * filtering to reduce noise in the delay time calculation.
     */
    void UpdateDelayTime(Fraction ratio);

    /** @brief Flag indicating a trigger event should be processed in the UI loop */
    bool do_trigger_ = false;

    /** @brief Edge detector for trigger input processing (rising edge) */
    EdgeDetector trigger_ = EdgeDetector(EdgeDetector::Type::RISING);

    bool inited_ = false;

        /** @brief LED colors for each knot type */
    EnumArray<Knoscil::KnotType, uint32_t> knot_colors_ = {
        WS2812::TEAL, ///< Color for TFOIL
        WS2812::LIME, ///< Color for LISSA
        WS2812::ORANGE ///< Color for TORUS
    };

    uint32_t current_knot_color_ = 0;

    enum class Pot
    {
        // commented out until we actually use them because the size of this enum
        // determines the size of the pots_ array.

        PITCH,       ///< Main pitch control (POT_5, Normal layer)
        PITCH_MOD,   ///< Pitch attenuversion amount (POT_1, Normal layer)
        TIMBRE,      ///< Timbre control - FM index (POT_6, Normal layer)
        TIMBRE_MOD,  ///< Timbre attenuversion amount (POT_2, Normal layer)
        ENV,         ///< Envelope control (POT_4, Normal layer)
        LFO,         ///< Rotation rate control (POT_7, Normal layer)
        LFO_MOD,     ///< Rotation rate modulation (POT_3, Normal layer)
        ENV_MOD,     ///< Envelope attenuversion amount (POT_4, Shift layer)
        FM_RATIO,    ///< FM ratio (POT_6, Shift layer)
        PITCH_SCALE, ///< Quantizer scale selection (POT_1, Mode layer)
        PITCH_ROOT,  ///< Root note selection (POT_2, Mode layer)
        PITCH_FINE,  ///< Fine pitch tuning (POT_3, Mode layer)
        FX,          ///< Delay effect wet/dry mix (POT_2, Shift layer)
        MODE_MOD,    ///< Mode attenuation control (POT_4, Mode layer)
        KNOT_P,      ///< Knot P value (POT_5, Mode layer)
        KNOT_Q,      ///< Knot Q value (POT_6, Mode layer)
        COUNT        ///< Total number of potentiometer controls
    };

    static constexpr Hardware::AnalogInput CV_TIMBRE = Hardware::AnalogInput::PARAM_1;     ///< CV input for timbre modulation
    static constexpr Hardware::AnalogInput CV_ENV = Hardware::AnalogInput::PARAM_3;        ///< CV input for envelope modulation
    static constexpr Hardware::AnalogInput CV_MODE = Hardware::AnalogInput::MODE;          ///< CV input for mode selection
    static constexpr Hardware::AnalogInput CV_PITCH_FREE = Hardware::AnalogInput::PITCH_1; ///< CV input for free pitch modulation
    static constexpr Hardware::AnalogInput CV_PITCH_NOTE = Hardware::AnalogInput::PITCH_2; ///< CV input for quantized pitch (V/Oct)
    static constexpr Hardware::AnalogInput CV_LFO_MOD = Hardware::AnalogInput::PARAM_2;    ///< CV input for LFO MOD
    static constexpr Hardware::DigitalInput TRIG_LFO_RESET = Hardware::DigitalInput::RESET_IN;

    EnumArray<Pot, std::unique_ptr<FancyPot>> pots_;

    /** @brief Mode selector for switching between synthesis modes with trigger-based input reading */
    FancyMode mode_selector_ = FancyMode(FancyMode::Config{
        .memory_addr = kMemMode,
        .modes_count = static_cast<uint32_t>(Mode::COUNT),
        .input_reading = FancyMode::InputReading::TRIGGERED});

    /**
     * @brief Memory addresses for persistent storage of values
     */
    static constexpr size_t kMemMode = Memory::ADDR_APP_SPACE + 0x0;       ///< Memory address for mode selection
    static constexpr size_t kMemFx = Memory::ADDR_APP_SPACE + 0x1;         ///< Memory address for FX (delay) setting
    static constexpr size_t kMemEnvMod = Memory::ADDR_APP_SPACE + 0x2;     ///< Memory address for envelope modulation setting
    static constexpr size_t kMemPitchScale = Memory::ADDR_APP_SPACE + 0x3; ///< Memory address for pitch scale setting
    static constexpr size_t kMemPitchRoot = Memory::ADDR_APP_SPACE + 0x4;  ///< Memory address for pitch root note setting
    static constexpr size_t kMemPitchFine = Memory::ADDR_APP_SPACE + 0x5;  ///< Memory address for fine pitch setting
    static constexpr size_t kMemModeMod = Memory::ADDR_APP_SPACE + 0x6;    ///< Memory address for mode modulation setting
    static constexpr size_t kMemModeKnotP = Memory::ADDR_APP_SPACE + 0x7;  ///< Memory address for Knot P setting
    static constexpr size_t kMemModeKnotQ = Memory::ADDR_APP_SPACE + 0x8;  ///< Memory address for Knot Q setting

    Mode mode_ = Mode::TFOIL_LISSA;
    Knoscil* knoscil_ = nullptr;
    Camera*  camera_ = nullptr;
    Knoscil::SampleType* out_data_ = nullptr;
    vessl::size_t out_data_read_ = 0;
    vessl::size_t out_data_write_ = 0;

    /** @brief Pitch quantizer for musical note quantization */
    Quantizer quantizer_;

    /** @brief Stored CV value for quantized pitch input (V/Oct) */
    int32_t pitch_note_cv_ = 0;

    /** @brief Current Y-Rotation value in q15 format */
    q15_t rot_y_value_ = 0;

    /** @brief ADSR envelope generator */
    AdsrEnv env_;

    /** @brief Current envelope value in q15 format */
    q15_t env_value_ = 0;

    /** @brief Flag indicating whether envelope is active (controlled by ENV pot position) */
    bool env_enabled_ = false;

    /** @brief StereoDelay effect applied post-envelope */
    StereoDelay stereo_delay_ = StereoDelay(StereoDelay::kMaxDelay / 2);

    q15_t delay_wet_ = Q15_ZERO;

    /** @brief Previous delay length value for noise filtering */
    uint32_t prev_stereo_delay_length_ = 0;
};
}
