// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../Core/DiceRateFormats.hpp"

#include <algorithm>

namespace ASFW::Audio::DICE::AllenHeath {

// Measured ZED-R16 layouts (#223); require matching live geometry.
[[nodiscard]] inline std::optional<DiceRateFormats>
ZedR16MeasuredFormats(const AudioStreamRuntimeCaps& caps) {
    if (caps.sampleRateHz != 44100 && caps.sampleRateHz != 48000 && caps.sampleRateHz != 88200 &&
        caps.sampleRateHz != 96000)
        return std::nullopt;
    const auto observed = DiceObservedFormat(caps);
    const auto mode = DiceRateMode(caps.sampleRateHz);
    if (!observed || !mode)
        return std::nullopt;
    DiceRateFormats measured{};
    measured[0] = DiceModeFormat{{{16, 16, 0, {}, 0}, {10, 10, 0, {}, 0}},
                                 {{16, 17, 1, {}, 1}, {10, 10, 0, {}, 0}}};
    measured[1] = DiceModeFormat{{{16, 16, 0, {}, 0}, {2, 2, 0, {}, 0}},
                                 {{16, 17, 1, {}, 1}, {2, 2, 0, {}, 0}}};
    const auto matches = [](const auto& observedStreams, const auto& measuredStreams) {
        return std::ranges::equal(
            observedStreams, measuredStreams,
            [](const auto& observedStream, const auto& measuredStream) {
                return observedStream.pcmChannels == measuredStream.pcmChannels &&
                       observedStream.dataBlockSize == measuredStream.dataBlockSize &&
                       observedStream.midiPortCount == measuredStream.midiPortCount;
            });
    };
    if (!matches(observed->playback, measured[*mode]->playback) ||
        !matches(observed->capture, measured[*mode]->capture))
        return std::nullopt;
    return measured;
}

[[nodiscard]] constexpr DiceRateFormatFallback ZedR16FormatFallback(uint64_t guid) noexcept {
    // Match OUI/category/product, excluding serial (Linux dice.c:34-68).
    return (guid >> 32U) == 0x0004c404U && (static_cast<uint32_t>(guid) >> 22U) == 0
        ? &ZedR16MeasuredFormats : nullptr;
}

} // namespace ASFW::Audio::DICE::AllenHeath
