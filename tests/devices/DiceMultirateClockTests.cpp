// SPDX-License-Identifier: Apache-2.0
#include "DICEDuplexTestSupport.hpp"
#include "Audio/Runtime/RateValidation.hpp"

using namespace ASFW::Testing::DICE;

TEST(DiceMultirateClockTests, BatchProgramsAndConfirmsEveryStandardRate) {
    static_assert(ASFW::Audio::Runtime::kDiceHardwareBatch);
    for (const auto& rate : ASFW::Audio::DICE::kDiceRateTable) {
        DuplexRig rig;
        uint32_t selection = 0;
        ASSERT_TRUE(ASFW::Audio::DICE::DiceClockSelectForRate(rate.hz, ClockSource::Internal, selection));
        const auto applied = rig.driver.ApplyClock({rate.hz, selection});
        ASSERT_TRUE(applied) << rate.hz << " status " << (applied ? 0 : applied.error());
        EXPECT_EQ(applied->appliedClock.sampleRateHz, rate.hz);
        EXPECT_EQ(applied->runtimeCaps.sampleRateHz, rate.hz);
        EXPECT_FALSE(rig.driver.IsRunning());
        EXPECT_EQ(rig.bus.Enable(), 0);
    }
}

TEST(DiceMultirateClockTests, RequestedHighRateRequiresActualReadback) {
    DuplexRig rig;
    rig.bus.SetClockSelectWriteHandler([&] {
        rig.bus.SetGlobalClockState(StatusBits::kSourceLocked |
            (ClockRateIndex::k48000 << StatusBits::kNominalRateShift), 48000, NotifyBits::kClockAccepted);
    });
    uint32_t selection = 0;
    ASSERT_TRUE(ASFW::Audio::DICE::DiceClockSelectForRate(96000, ClockSource::Internal, selection));
    const auto applied = rig.driver.ApplyClock({96000, selection});
    // Acceptance at the old achieved rate cannot prepare the next StartIO.
    // Refuse the transaction instead of publishing an unready clock.
    ASSERT_FALSE(applied);
    EXPECT_EQ(applied.error(), kIOReturnTimeout);
    EXPECT_FALSE(rig.driver.IsRunning());
    EXPECT_EQ(rig.bus.Enable(), 0);
}

TEST(DiceMultirateClockTests, ExternalClockUsesConfiguredRateWithoutLockOrWithCounterDrift) {
    using namespace ASFW::Audio::DICE;
    DuplexRig rig({.allowExternalClock = true});
    for (const auto rate : {48000U, 96000U, 48000U}) {
        uint32_t selection = 0;
        ASSERT_TRUE(DiceClockSelectForRate(rate, ClockSource::ADAT, selection));
        rig.bus.SetClockSelectWriteHandler([&] {
            // Last measured external frequency can belong to the other rate mode.
            rig.bus.SetGlobalClockState(ClockRateIndex::k96000 << StatusBits::kNominalRateShift,
                                       96093, NotifyBits::kClockAccepted);
        });
        const auto applied = rig.driver.ApplyClock({rate, selection});
        ASSERT_TRUE(applied) << rate << " status " << (applied ? 0 : applied.error());
        EXPECT_EQ(applied->appliedClock.sampleRateHz, rate);
        EXPECT_EQ(applied->runtimeCaps.sampleRateHz, rate);
        EXPECT_EQ(rig.bus.Enable(), 0);
    }
    const auto locked96 = StatusBits::kSourceLocked | (ClockRateIndex::k96000 << StatusBits::kNominalRateShift);
    EXPECT_EQ(DiceConfiguredRate(96093, locked96, 0x405, true), 96000U);
    EXPECT_EQ(DiceConfiguredRate(96093, locked96, 0x205, true), 0U);
    EXPECT_EQ(DiceConfiguredRate(96093, locked96, 0x405, false), 96093U);
    EXPECT_EQ(DiceConfiguredRate(96093, locked96, 0x40c, true), 96093U);
}

TEST(DiceMultirateClockTests, UnlockedExternalClockCanPrepareAndEnableDuplex) {
    using namespace ASFW::Audio::DICE;
    DuplexRig rig({.allowExternalClock = true});
    rig.bus.SetClockSelectWriteHandler([&] { rig.bus.SetGlobalClockState(0, 96000, NotifyBits::kClockAccepted); });
    const ASFW::Audio::AudioDuplexChannels channels{.deviceToHostIsoChannel = 1, .hostToDeviceIsoChannel = 0};
    const auto prepared = rig.driver.Prepare(channels, {48000, 0x205}, true);
    ASSERT_TRUE(prepared) << (prepared ? 0 : prepared.error());
    ASSERT_TRUE(rig.driver.ProgramRx());
    ASSERT_TRUE(rig.driver.ProgramTxAndEnable());
    ASSERT_TRUE(rig.driver.Confirm());
    EXPECT_TRUE(rig.driver.IsRunning());
    EXPECT_EQ(rig.driver.Stop(), kIOReturnSuccess);
}

TEST(DiceMultirateClockTests, ExternalSourceSupportRemainsOptIn) {
    DuplexRig rig;
    EXPECT_FALSE(rig.driver.ApplyClock({48000, 0x205}));
    EXPECT_EQ(rig.bus.Enable(), 0);
}

TEST(DiceMultirateClockTests, ExternalOptInDoesNotRelaxInternalLockRequirement) {
    using namespace ASFW::Audio::DICE;
    DuplexRig rig({.allowExternalClock = true});
    rig.bus.SetClockSelectWriteHandler([&] {
        rig.bus.SetGlobalClockState(ClockRateIndex::k48000 << StatusBits::kNominalRateShift,
                                   48000, NotifyBits::kClockAccepted);
    });
    const auto applied = rig.driver.ApplyClock({48000, 0x20c});
    EXPECT_FALSE(applied);
    EXPECT_EQ(rig.bus.Enable(), 0);
}
