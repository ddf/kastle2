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

void AppKnoscillator::Init()
{
    inited_ = false;
    mode_ = Mode::FIRST;
    knoscil_ = Knoscil::create(SAMPLE_RATE);
    knoscil_->frequency() = 220.f;

    outData = new Knoscil::SampleType[I2S::kAudioBufferSize];

    // disable audio chain, we are doing it ourselves
    Kastle2::base.SetFeatureEnabled(Base::Feature::AUDIO_CHAIN, false);

    // Mode selection
    mode_selector_.Init();

    pots_[Pot::MODE_MOD] = FancyPot::Create({.pot = Hardware::Pot::POT_4,
                                            .layer = Hardware::Layer::MODE,
                                            .initial_value = kModeModDefaultValue,
                                            .memory_addr = kMemModeMod});

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

    // Detect the trigger and do dirty inputs
    if (trigger_.Process(Kastle2::hw.GetTriggerIn()))
    {
        do_trigger_ = true;
    }
    
    vessl::array<Knoscil::SampleType> buf(outData, size);
    knoscil_->generate(buf);

    Knoscil::SampleType samp;
    vessl::array<q15_t> out(output, size*2);
    auto reader = buf.getReader();
    auto writer = out.getWriter();
    while(writer.available())
    {
        samp = reader.read();
        writer << q31_to_q15(vessl::cast<q31_t>(samp.left())) 
               << q31_to_q15(vessl::cast<q31_t>(samp.right()));
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
    gDbg.x = coord.x.v_;
    gDbg.y = coord.y.v_;
    gDbg.z = coord.z.v_;
    gDbg.cz = vessl::cast<vessl::analog_t>(coord.z);
    gDbg.proj = knoscil_->getProjection().v_;
}

void AppKnoscillator::Trigger()
{
    // Store the current note
    // Ideally you'd like to use DirtyInputHandler to make sure you have the latest value
    // But it's OK for this example
    //pitch_note_cv_ = Kastle2::hw.GetAnalogValue(CV_PITCH_NOTE);

    // Store current mode
    mode_selector_.TriggerAdcRead();

    // Trigger envelope
    //env_.Trigger();
}

void AppKnoscillator::UiLoop()
{
    // Handle mode switching
    mode_selector_.ReadValue();
    mode_ = static_cast<Mode>(mode_selector_.GetMode());

    // Change LED color based on mode
    switch (mode_)
    {
    case Mode::FIRST:
        knoscil_->knotTypeA() = KnotType::TFOIL;
        knoscil_->knotTypeB() = KnotType::LISSA;
        break;

    case Mode::SECOND:
        knoscil_->knotTypeA() = KnotType::LISSA;
        knoscil_->knotTypeB() = KnotType::TORUS;
        break;

    case Mode::THIRD:
        knoscil_->knotTypeA() = KnotType::TORUS;
        knoscil_->knotTypeB() = KnotType::TFOIL;
        break;
    }

    // Update pots
    for (auto &pot : pots_)
    {
        pot->ReadValue();
    }

    if (pots_[Pot::MODE_MOD]->HasChanged())
    {
        // ideally we'd expand pot rang to phase_t without going thru float, but my brain can't do that right now.
        float morph = static_cast<float>(pots_[Pot::MODE_MOD]->GetValue()) / POT_MAX;
        knoscil_->knotMorph() = morph;

        auto colorA = knot_colors_[knoscil_->knotTypeA().read<KnotType>()];
        auto colorB = knot_colors_[knoscil_->knotTypeB().read<KnotType>()];
        auto color = WS2812::CrossfadeColors(colorA, colorB, morph*255 + 0.5f);

        Kastle2::hw.SetLed(Hardware::Led::LED_1, color);
        Kastle2::hw.SetLed(Hardware::Led::LED_2, color);
    }
}

void AppKnoscillator::MidiCallback(midi::Message *msg)
{
    // Do something here...
    (void)msg;
}
