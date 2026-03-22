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

using namespace kastle2;
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

    //Kastle2::debug.SetEnabled(true);

    inited_ = true;
}

void AppKnoscillator::DeInit()
{
    inited_ = false;
    Knoscil::destroy(knoscil_);
    delete[] outData;
}

FASTCODE void AppKnoscillator::AudioLoop(q15_t *input, q15_t *output, size_t size)
{
    if (!inited_)
    {
        return;
    }

    // silence unused parameter warning.
    (void)input;
    
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

void AppKnoscillator::UiLoop()
{
    // Change LED color based on mode
    switch (mode_)
    {
    case Mode::FIRST:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 255, 255, 255);
        knoscil_->knotTypeA() = KnotType::TFOIL;
        knoscil_->knotTypeB() = KnotType::LISSA;
        break;

    case Mode::SECOND:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 0, 255, 0);
        knoscil_->knotTypeA() = KnotType::LISSA;
        knoscil_->knotTypeB() = KnotType::TORUS;
        break;

    case Mode::THIRD:
        Kastle2::hw.SetLed(Hardware::Led::LED_2, 255, 0, 0);
        knoscil_->knotTypeA() = KnotType::TORUS;
        knoscil_->knotTypeB() = KnotType::TFOIL;
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

void AppKnoscillator::MidiCallback(midi::Message *msg)
{
    // Do something here...
    (void)msg;
}
