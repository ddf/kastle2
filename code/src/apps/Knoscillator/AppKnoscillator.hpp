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
#include "common/core/App.hpp"
#include "common/core/Hardware.hpp"
#include "common/core/Kastle2.hpp"
#include "common/dsp/math/qmath.hpp"
#include "vessicle/Knoscillator.h"

namespace kastle2
{

class AppKnoscillator : public virtual App
{
    using Knoscil = Knoscillator<float>;
public:
    /**
     * @brief Some example modes.
     */
    enum class Mode
    {
        FIRST,
        SECOND,
        THIRD,
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
     * @brief Called when the app is first loaded - initializes the memory values.
     */
    void MemoryInitialization() override {}

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
    bool inited_ = false;
    Mode mode_ = Mode::FIRST;
    Knoscil* knoscil_ = nullptr;
};
}
