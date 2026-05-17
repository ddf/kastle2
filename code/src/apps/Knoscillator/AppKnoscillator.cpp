/*
MIT License

Copyright (c) 2026 Damien Quartz
Copyright (c) 2024 Marek Mach (Bastl Instruments)

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

#include "AppKnoscillator.hpp"
#include "common/core/Kastle2.hpp"
#include "common/utils.hpp"
#include "vessl_kqmath.hpp"
#include "KnoscillatorParameterMaps.hpp"

using namespace kastle2;
using namespace knoscillator;
using KnotType = Knoscil::KnotType;

KnotDebug gDbg;

static constexpr size_t outDataSize = I2S::kAudioBufferSize*2;

void AppKnoscillator::Init()
{
    inited_ = false;
    mode_ = Mode::TFOIL_LISSA;
    knoscil_ = Knoscil::create(SAMPLE_RATE);
    knoscil_->frequency() = 220.f;

    outData = new Knoscil::SampleType[outDataSize];
    outData_read_ = 0;
    outData_write_ = 0;

    // disable audio chain, we are doing it ourselves
    Kastle2::base.SetFeatureEnabled(Base::Feature::AUDIO_CHAIN, false);

    // Quantizer
    quantizer_.Init(0.8f);
    quantizer_.SetEnabled(true);
    quantizer_.SetScale(Quantizer::DefaultScale::CHROMATIC);

    // Envelope
    env_.Init(SAMPLE_RATE);
    env_.SetAttackTime(0.01f);
    env_.SetDecayTime(1.0f);
    env_.SetNonResetting(AdsrEnv::NonResetting::DECAY); // prevents clicks
    env_value_ = 0;
    env_enabled_ = false;

    stereo_delay_.Init(SAMPLE_RATE);
    stereo_delay_.SetFeedback(q15(0.15f));
    stereo_delay_.SetFilterEnabled(true);
    stereo_delay_.SetFilterResonance(0.6f);
    UpdateDelayTime(kDelayRatio);

    // Allow ENV OUT to be controlled by this App
    Kastle2::base.SetFeatureEnabled(Base::Feature::ENV_OUT, false);
    // Allow LFO to be controlled by this App
    Kastle2::base.SetFeatureEnabled(Base::Feature::LFO_OUT, false);

    // Mode selection
    mode_selector_.Init();

    pots_[Pot::PITCH] = FancyPot::Create({
        .pot = Hardware::Pot::POT_5,
        .layer = Hardware::Layer::NORMAL,
        .freeze = true,
    });

    pots_[Pot::PITCH_MOD] = FancyPot::Create({
        .pot = Hardware::Pot::POT_1,
        .layer = Hardware::Layer::NORMAL,
        .deadzone = true,
        .freeze = true,
    });

    pots_[Pot::ENV] = FancyPot::Create({ 
        .pot = Hardware::Pot::POT_4,
        .layer = Hardware::Layer::NORMAL,
        .deadzone = true,
    });

    pots_[Pot::TIMBRE] = FancyPot::Create({
        .pot = Hardware::Pot::POT_6,                             
        .layer = Hardware::Layer::NORMAL
    });

    pots_[Pot::TIMBRE_MOD] = FancyPot::Create({
        .pot = Hardware::Pot::POT_2,
        .layer = Hardware::Layer::NORMAL,
        .deadzone = true,
    });

    pots_[Pot::LFO] = FancyPot::Create({
        .pot = Hardware::Pot::POT_7,
        .layer = Hardware::Layer::NORMAL,
        .deadzone = true,
    });

    pots_[Pot::LFO_MOD] = FancyPot::Create({
        .pot = Hardware::Pot::POT_3,
        .layer = Hardware::Layer::NORMAL,
        .deadzone = true
    });

    // Shift layer
    pots_[Pot::ENV_MOD] = FancyPot::Create({
        .pot = Hardware::Pot::POT_4,
        .layer = Hardware::Layer::SHIFT,
        .initial_value = kEnvModDefaultValue,
        .deadzone = true
    });

    pots_[Pot::FX] = FancyPot::Create({
        .pot = Hardware::Pot::POT_2,
        .layer = Hardware::Layer::SHIFT,
        .initial_value = kFxDefaultValue,
        .deadzone = true,
        .memory_addr = kMemFx
    });

    pots_[Pot::FM_RATIO] = FancyPot::Create({
        .pot = Hardware::Pot::POT_6,
        .layer = Hardware::Layer::SHIFT,
        .initial_value = kResonanceDefaultValue,
        .deadzone = true
    });

    // Mode layer
    pots_[Pot::PITCH_SCALE] = FancyPot::Create({
        .pot = Hardware::Pot::POT_1,
        .layer = Hardware::Layer::MODE,
        .initial_value = kPitchScaleDefaultValue,
        .map_size = quantizer_.GetScaleTableSize(), // quantizer needs to be initialized before calling this
        .memory_addr = kMemPitchScale
    });

    pots_[Pot::PITCH_ROOT] = FancyPot::Create({
        .pot = Hardware::Pot::POT_2,
        .layer = Hardware::Layer::MODE,
        .initial_value = kPitchRootDefaultValue,
        .map_size = Quantizer::kMultiplierTable.size(),
        .memory_addr = kMemPitchRoot
    });

    pots_[Pot::PITCH_FINE] = FancyPot::Create({
        .pot = Hardware::Pot::POT_3,
        .layer = Hardware::Layer::MODE,
        .initial_value = kPitchFineDefaultValue,
        .deadzone = true,
        .memory_addr = kMemPitchFine
    });

    pots_[Pot::MODE_MOD] = FancyPot::Create({
        .pot = Hardware::Pot::POT_4,
        .layer = Hardware::Layer::MODE,
        .initial_value = kModeModDefaultValue,
        .memory_addr = kMemModeMod
    });

    pots_[Pot::KNOT_P] = FancyPot::Create({
        .pot = Hardware::Pot::POT_5,
        .layer = Hardware::Layer::MODE,
        .initial_value = kKnotPDefaultValue,
        .memory_addr = kMemModeKnotP
    });

    pots_[Pot::KNOT_Q] = FancyPot::Create({
        .pot = Hardware::Pot::POT_6,
        .layer = Hardware::Layer::MODE,
        .initial_value = kKnotQDefaultValue,
        .memory_addr = kMemModeKnotQ
    });

    // Pots need to be initialized
    for (auto &pot : pots_)
    {
        pot->Init(AUDIO_LOOP_RATE);
    }

    // This disables the next change when the pots are moved
    // or when the current layer time is over the specified number of ticks
    mode_selector_.DisableNextChangeWhen(pots_, kModeShortPressUnder);

    inited_ = true;
}

void AppKnoscillator::DeInit()
{
    inited_ = false;
    Knoscil::destroy(knoscil_);
    delete[] outData;
}

FASTCODE void AppKnoscillator::AudioLoop([[maybe_unused]]q15_t *input, q15_t *output, size_t size)
{
    if (!inited_)
    {
        return;
    }

    // request samples
    MultiCore::SendMessage(MultiCore::MessageType::SAMPLE_REQUEST);

    // Detect the trigger and do dirty inputs
    if (trigger_.Process(Kastle2::hw.GetTriggerIn()))
    {
        do_trigger_ = true;
    }

    // continue when samples are ready
    MultiCore::WaitForMessage(MultiCore::MessageType::DONE);

    // capture and convert rotation to q15 [0,1]
    q31_t rotY = vessl::cast<q31_t>(knoscil_->rotationY().read<vessl::q31>());
    rot_y_value_ = q31_to_q15(q31_add(q31_mult(rotY, Q31_HALF), Q31_HALF));

    // kick off second core on next buffer while we process the last one it created
    MultiCore::SendMessage(MultiCore::MessageType::BEGIN, size);

    Knoscil::SampleType samp;
    vessl::array<q15_t> out(output, size*2);
    auto writer = out.getWriter();
    while(writer.available())
    {
        samp = outData[outData_read_++];

        q15_t lout = q31_to_q15(vessl::cast<q31_t>(samp.left()));
        q15_t rout = q31_to_q15(vessl::cast<q31_t>(samp.right()));

        // Calculate the envelope
        env_value_ = q31_to_q15(env_.Process());

        if (env_enabled_)
        {
            lout = q15_mult(lout, env_value_);
            rout = q15_mult(rout, env_value_);
        }

        auto dout = stereo_delay_.Process(lout, rout);
        writer << dout.left << dout.right;

        if (outData_read_ == outDataSize)
        {
            outData_read_ = 0;
        }
    }

    // Process pots in audio-rate
    for (auto &pot : pots_)
    {
        pot->Process();
    }

    gDbg.pp = knoscil_->knot().pp();
    gDbg.pq = knoscil_->knot().pq();
    gDbg.pz = knoscil_->knot().pz();
    gDbg.left = samp.left().v_;
    gDbg.right = samp.right().v_;

    auto coord = knoscil_->knot().xyz();
    gDbg.kx = coord.x.v_;
    gDbg.ky = coord.y.v_;
    gDbg.kz = coord.z.v_;
    //gDbg.rr = knoscil_->rotator.rInc.v_;
    //gDbg.ryf = knoscil_->rotator.params.ratioY.value;
    gDbg.ryfa = vessl::cast<vessl::analog_t>(gDbg.ryf);
    gDbg.cx = vessl::cast<vessl::analog_t>(coord.x);
    gDbg.cy = vessl::cast<vessl::analog_t>(coord.y);
    gDbg.cz = vessl::cast<vessl::analog_t>(coord.z);
    gDbg.proj = knoscil_->getProjection().v_;
}

void knoscillator::AppKnoscillator::SecondCoreGenerate(size_t size)
{
    vessl::array<Knoscil::SampleType> buf(outData + outData_write_, size);
    knoscil_->generate(buf);
    outData_write_ = (outData_write_ + size) % outDataSize;
}

void AppKnoscillator::Trigger()
{
    // Store the current note
    // Ideally you'd like to use DirtyInputHandler to make sure you have the latest value
    // But it's OK for this example
    pitch_note_cv_ = Kastle2::hw.GetAnalogValue(CV_PITCH_NOTE);

    // Store current mode
    mode_selector_.TriggerAdcRead();

    // Trigger envelope
    env_.Trigger();
}

void AppKnoscillator::UiLoop()
{
    if (do_trigger_)
    {
        Trigger();
        do_trigger_ = false;
    }

    // Handle mode switching
    mode_selector_.ReadValue();
    Mode prevMode = mode_;
    mode_ = static_cast<Mode>(mode_selector_.GetMode());

    // Change LED color based on mode
    switch (mode_)
    {
    case Mode::TFOIL_LISSA:
        knoscil_->knotTypeA() = KnotType::TFOIL;
        knoscil_->knotTypeB() = KnotType::LISSA;
        break;

    case Mode::LISSA_TORUS:
        knoscil_->knotTypeA() = KnotType::LISSA;
        knoscil_->knotTypeB() = KnotType::TORUS;
        break;

    case Mode::TORUS_TFOIL:
        knoscil_->knotTypeA() = KnotType::TORUS;
        knoscil_->knotTypeB() = KnotType::TFOIL;
        break;
    }

    // Update pots
    for (auto &pot : pots_)
    {
        pot->ReadValue();
    }

    // Quantizer Scale selection based on the pot value
    int32_t quantizer_scale = pots_[Pot::PITCH_SCALE]->GetMappedValue();
    quantizer_.SetScale(quantizer_scale >= 0 ? quantizer_scale : 0);

    // Raw pitch value from the pot
    float base_pitch = std::pow(2.0f, curve_map(pots_[Pot::PITCH]->GetValue(), kMapFreePitch));

    // Apply NOTE PITCH mod
    int32_t pitch_mod_pot = pots_[Pot::PITCH_MOD]->GetValue();
    int32_t pitch_note_mod = apply_pot_mod_attenuvert(pitch_note_cv_, pitch_mod_pot);
    base_pitch *= std::pow(2.0f, static_cast<float>(pitch_note_mod) / static_cast<float>(ADC_1V)); // V/Oct

    // Multiply by base frequency
    base_pitch *= kBaseTune;

    // Apply quantization
    base_pitch = quantizer_.Process(base_pitch);

    // Apply quantization root note
    int32_t quantizer_root = pots_[Pot::PITCH_ROOT]->GetMappedValue();
    base_pitch *= Quantizer::kMultiplierTable[quantizer_root];

    // Apply FREE PITCH mod
    int32_t pitch_free_mod = apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_PITCH_FREE), pitch_mod_pot);
    base_pitch *= std::pow(2.0f, static_cast<float>(pitch_free_mod) / static_cast<float>(ADC_1V)); // V/Oct

    // Add fine tuning
    base_pitch *= curve_map(pots_[Pot::PITCH_FINE]->GetValue(), kMapPitchFine);

    // Clamp the frequency
    base_pitch = fmin(base_pitch, kMaxPitchHz);

    // Calculate timbre and fm ratio settings
    int32_t timbre_val = pots_[Pot::TIMBRE]->GetValue();
    timbre_val += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_TIMBRE), pots_[Pot::TIMBRE_MOD]->GetValue());
    int32_t fm_ratio_val = pots_[Pot::FM_RATIO]->GetValue();
    float fm_index = q31_to_float(curve_map(timbre_val, kMapFmIndex, MapClamp::TRUE, MapSafe::TRUE));
    float fm_ratio = 8.f * q31_to_float(curve_map(fm_ratio_val, kMapFmRatio, MapClamp::TRUE, MapSafe::TRUE));

    // Calculate rotation settings
    int32_t rot_val = pots_[Pot::LFO]->GetValue();
    rot_val += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_LFO_MOD), pots_[Pot::LFO_MOD]->GetValue());
    float rot_ratio = q31_to_float(curve_map(rot_val, kMapRotRatio, MapClamp::TRUE, MapSafe::TRUE));

    // removing volatile keyword here makes knot_p and knot_q get set to 0 for some reason?
    volatile int32_t knot_p_val = pots_[Pot::KNOT_P]->GetValue();
    volatile int32_t knot_q_val = pots_[Pot::KNOT_Q]->GetValue();
    volatile int32_t knot_p = curve_map(knot_p_val, kMapKnotPQ, MapClamp::TRUE, MapSafe::TRUE);
    volatile int32_t knot_q = curve_map(knot_q_val, kMapKnotPQ, MapClamp::TRUE, MapSafe::TRUE);

    knoscil_->frequency() = base_pitch;
    knoscil_->fmIndex() = fm_index;
    knoscil_->fmRatio() = fm_ratio;
    // chosen on vibes by playing around with it
    knoscil_->rotRatioY() = rot_ratio;
    knoscil_->rotRatioX() = -rot_ratio / 4;
    knoscil_->rotRatioZ() = rot_ratio / 8;
    knoscil_->knotP() = knot_p;
    knoscil_->knotQ() = knot_q;

    if (Kastle2::hw.GetDigitalIn(TRIG_LFO_RESET))
    {
        knoscil_->resetRotation();
    }

     // Calculate envelope
    int32_t env_val = pots_[Pot::ENV]->GetValue();
    env_val += apply_pot_mod_attenuvert(Kastle2::hw.GetAnalogValue(CV_ENV), pots_[Pot::ENV_MOD]->GetValue());
    env_.SetAttackTime(curve_map(env_val, kMapEnvAttack, MapClamp::TRUE));
    env_.SetDecayTime(curve_map(env_val, kMapEnvDecay, MapClamp::TRUE));
    if (pots_[Pot::ENV]->HasChanged())
    {
        env_enabled_ = (env_val > pot(0.05f)) && (env_val < pot(0.95f));
    }

    // Pass the calculated envelope into ENV output
    Kastle2::hw.SetEnvOut(((uint32_t)env_value_) >> (15 - 10));
    // Pass the captured rotation to the LFO TRI output
    Kastle2::hw.SetTriOut(((uint32_t)rot_y_value_) >> (15 - 10));
    Kastle2::hw.SetPulseOut(rot_y_value_ > Q15_HALF);

    // Update delay
    int32_t fx_value = pots_[Pot::FX]->GetValue();
    q15_t delay_wet = curve_map(fx_value, kMapFxDelayWet, MapClamp::TRUE, MapSafe::TRUE);
    q15_t delay_fbk = curve_map(fx_value, kMapFxDelayFeed, MapClamp::TRUE, MapSafe::TRUE);
    q15_t delay_flt = curve_map(fx_value, kMapFxDelayFilter, MapClamp::TRUE, MapSafe::TRUE);
    stereo_delay_.SetWet(delay_wet);
    stereo_delay_.SetFeedback(delay_fbk);
    stereo_delay_.SetFilterCrossfade(delay_flt);
    UpdateDelayTime({knot_p, knot_q});

    if (current_knot_color_ == 0 || mode_ != prevMode || pots_[Pot::MODE_MOD]->HasChanged())
    {   
        // ideally we'd expand pot range to phase_t without going thru float, but my brain can't do that right now.
        float morph = static_cast<float>(pots_[Pot::MODE_MOD]->GetValue()) / POT_MAX;
        knoscil_->knotMorph() = vessl::cast<vessl::phase_t>(morph);

        auto colorA = knot_colors_[knoscil_->knotTypeA().read<KnotType>()];
        auto colorB = knot_colors_[knoscil_->knotTypeB().read<KnotType>()];
        current_knot_color_ = WS2812::CrossfadeColors(colorA, colorB, morph*255 + 0.5f);
    }

    uint8_t bright = (env_value_ >> (15 - 6)) + 63;
    uint32_t color = WS2812::ApplyBrightness(current_knot_color_, bright);
    Kastle2::hw.SetLed(Hardware::Led::LED_1, color);
    Kastle2::hw.SetLed(Hardware::Led::LED_2, color);
}

FASTCODE void knoscillator::AppKnoscillator::SecondCoreWorker()
{
    while (inited_)
    {
        if (MultiCore::HasMessage())
        {
            // Monitoring second core performance
            //Kastle2::hw.SetDebugPin(1, 1);

            MultiCore::Message m = MultiCore::GetMessage();
            switch (m.type)
            {
            case MultiCore::MessageType::BEGIN:
                SecondCoreGenerate(m.data);
                break;

            case MultiCore::MessageType::SAMPLE_REQUEST:
                MultiCore::SendMessage(MultiCore::MessageType::DONE);
                break;
            }

            //Kastle2::hw.SetDebugPin(1, 0);
        }
    }
}

void AppKnoscillator::UpdateDelayTime(Fraction ratio)
{
    size_t stereo_delay_length = ((Kastle2::base.GetClock().GetAverageTargetTicks() * AUDIO_BUFFER_SIZE) * ratio.n) / ratio.d;
    // Filter out noise
    if (diff(stereo_delay_length, prev_stereo_delay_length_) < 10)
    {
        return;
    }
    prev_stereo_delay_length_ = stereo_delay_length;
    stereo_delay_.SetDelay(std::max(int(stereo_delay_length) + 80, 5), std::max(int(stereo_delay_length) - 80, 5));
}

void AppKnoscillator::MidiCallback(midi::Message *msg)
{
    // Do something here...
    (void)msg;
}

void knoscillator::AppKnoscillator::MemoryInitialization()
{
    Kastle2::memory.Write8(kMemMode, std::to_underlying(Mode::TFOIL_LISSA));
    Kastle2::memory.Write8(kMemFx, pot_to_mem(kFxDefaultValue));
    Kastle2::memory.Write8(kMemEnvMod, pot_to_mem(kEnvModDefaultValue));
    Kastle2::memory.Write8(kMemPitchScale, pot_to_mem(kPitchScaleDefaultValue));
    Kastle2::memory.Write8(kMemPitchRoot, pot_to_mem(kPitchRootDefaultValue));
    Kastle2::memory.Write8(kMemPitchFine, pot_to_mem(kPitchFineDefaultValue));
    Kastle2::memory.Write8(kMemModeMod, pot_to_mem(kModeModDefaultValue));
    Kastle2::memory.Write8(kMemModeKnotP, pot_to_mem(kKnotPDefaultValue));
    Kastle2::memory.Write8(kMemModeKnotQ, pot_to_mem(kKnotQDefaultValue));
}
