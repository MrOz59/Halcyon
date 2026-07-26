#include <catch2/catch.hpp>

#include <SynchronizedClock.hpp>

#include <chrono>
#include <thread>

TEST_CASE("Synchronized clock never moves backward", "[network.clock]")
{
    using namespace std::chrono_literals;

    TiltedPhoques::SynchronizedClock clock;
    REQUIRE_FALSE(clock.IsSynchronized());

    // Start with an intentionally large RTT estimate, then sharply lower it.
    // The resulting negative clock correction used to make interpolation time
    // move backward while the correction was being applied.
    clock.Synchronize(10'000, 4'000);
    const auto firstTick = clock.GetCurrentTick();
    REQUIRE(firstTick == 12'000);

    clock.Synchronize(10'001, 0);
    std::this_thread::sleep_for(25ms);
    clock.Update();

    REQUIRE(clock.GetCurrentTick() >= firstTick);
}

TEST_CASE("Unsynchronized clock does not underflow the interpolation delay", "[network.clock]")
{
    // RunRemoteUpdates subtracts a fixed interpolation delay from the current
    // tick. The clock reads 0 until the first server time sync arrives, so the
    // subtraction has to be clamped instead of wrapping around uint64_t.
    constexpr uint64_t cInterpolationDelay = 300;
    const auto delayedTick = [](uint64_t aTick)
    { return aTick > cInterpolationDelay ? aTick - cInterpolationDelay : 0; };

    TiltedPhoques::SynchronizedClock clock;
    REQUIRE_FALSE(clock.IsSynchronized());
    REQUIRE(clock.GetCurrentTick() == 0);
    REQUIRE(delayedTick(clock.GetCurrentTick()) == 0);

    // Below, at, and above the delay must all stay in range.
    REQUIRE(delayedTick(0) == 0);
    REQUIRE(delayedTick(299) == 0);
    REQUIRE(delayedTick(300) == 0);
    REQUIRE(delayedTick(301) == 1);

    // A synchronized clock keeps the original behaviour.
    clock.Synchronize(10'000, 0);
    REQUIRE(clock.GetCurrentTick() == 10'000);
    REQUIRE(delayedTick(clock.GetCurrentTick()) == 9'700);
}
