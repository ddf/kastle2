/*
MIT License

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

#include "AppVessicle.hpp"
#include "common/core/Kastle2.hpp"
#include "common/utils.hpp"
#include "vessl_kqmath.hpp"

using namespace kastle2;

static volatile vessl::phase_t gOscilPhase;
static volatile vessl::phase_t gOscilInc;

void AppVessicle::Init()
{
    inited_ = false;
    mode_ = Mode::FIRST;
    
    voscil_.setSampleRate(SAMPLE_RATE);
    voscil_.fHz() = 220.f;

    koscil_.Init(SAMPLE_RATE);
    koscil_.SetFrequency(voscil_.fHz().readAnalog());

    knotOscil_ = KnotOscil::create(SAMPLE_RATE);
    knotOscil_->frequency() = voscil_.fHz();

    // disable audio chain, we are doing it ourselves
    Kastle2::base.SetFeatureEnabled(Base::Feature::AUDIO_CHAIN, false);

    inited_ = true;
}

void AppVessicle::DeInit()
{
    delete knotOscil_;
    inited_ = false;
}

FASTCODE void AppVessicle::AudioLoop(q15_t *input, q15_t *output, size_t size)
{
    if (!inited_)
    {
        return;
    }
    vessl::q31 qfreq((int64_t)(voscil_.fHz().readAnalog() * (INT32_MAX / SAMPLE_RATE)));
    vessl::phase_t inc = vessl::cast<vessl::phase_t>(qfreq);
    for (size_t i = 0; i < size; i++)
    {
        // read
        q15_t left = input[2 * i];
        q15_t right = input[2 * i + 1];

        // code that runs each sample
        q31_t ks = koscil_.Process();
        vessl::q31 vs = voscil_.generate();
        
        left = q31_to_q15(ks);
        //right = q31_to_q15(vessl::math::sin<q31_t>(vessl::cast<vessl::phase_t>(koscil_.GetPhase())));
        
        // this works, runs at the same frequency as koscil,
        // but our sine is a cosine relative to koscil's sine.
        //right = q31_to_q15(vessl::cast<q31_t>(vs));

        // this works, but runs at half the frequency of koscil
        // because in their code they run phase at 2x speed.
        //right = q31_to_q15(vessl::math::sin<q31_t>(phase));

        // this works
        //right = q31_to_q15(vessl::math::sin<q31_t>(vessl::cast<vessl::phase_t>(koscil_.GetPhase())));

        //phase.v_ += inc.v_;
        //phase.accum(inc);

        // if all of the above works, this should work.
        KnotOscil::coord_t xyz = knotOscil_->generate<false>();
        left = q31_to_q15(vessl::cast<q31_t>(xyz.x));
        right = q31_to_q15(vessl::cast<q31_t>(xyz.y));

        // output
        output[2 * i] = left;
        output[2 * i + 1] = right;
    }

    gOscilPhase = phase; // oscil_.getPhase().v_;
    gOscilInc = inc; // oscil_.getInc().v_;
}

void AppVessicle::UiLoop()
{
    // Change LED color based on mode
    switch (mode_)
    {
    case Mode::FIRST:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 255, 255, 255);
        break;

    case Mode::SECOND:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 0, 255, 0);
        break;

    case Mode::THIRD:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 255, 0, 0);
        break;
    }

    // Cycle modes
    if (Kastle2::hw.JustReleased(Hardware::Button::MODE))
    {
        mode_ = EnumIncrement(mode_);
    }

    // set kastle LED
    Kastle2::hw.SetLed(Hardware::Led::LED_1, 0xFF8090);
}

void AppVessicle::MidiCallback(midi::Message *msg)
{
    // Do something here...
    (void)msg;
}
