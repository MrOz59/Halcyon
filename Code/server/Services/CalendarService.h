#pragma once

#include <Events/PacketEvent.h>
#include <DateTime.h>
#include <Structs/GameId.h>

struct World;
struct UpdateEvent;
struct PlayerJoinEvent;

/**
 * @brief Manages time and date of the world.
 */
class CalendarService
{
public:
    CalendarService(World& aWorld, entt::dispatcher& aDispatcher);

    // we use these types for SOL
    // this is done this way because SOL
    // provides direct support for these
    using TTime = std::pair<int, int>;
    using TDate = std::tuple<int, int, int>;

    bool SetTime(int aHour, int aMinutes, float aScale) noexcept;
    bool SetDate(int aDay, int aMonth, float aYear) noexcept;

    // returns hours, minutes
    TTime GetTime() const noexcept;
    static TTime GetRealTime() noexcept;

    // returns dd/mm/yy
    TDate GetDate() const noexcept;

    float GetTimeScale() const noexcept { return m_dateTime.m_timeModel.TimeScale; }
    bool SetTimeScale(float aScale) noexcept;

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnPlayerJoin(const PlayerJoinEvent&) noexcept;
    void SendTimeResync() noexcept;

    DateTime m_dateTime;
    // Own monotonic reference instead of Server::GetTick(): that value is
    // milliseconds on the transport's clock epoch, so the first delta after
    // startup is however long the machine has been up. Advancing the calendar by
    // that would jump it far ahead once, and every client connecting afterwards
    // would be handed a different time than the ones already in.
    std::chrono::steady_clock::time_point m_lastUpdate{};
    // Clients receive the time once on connect and then integrate it locally, so
    // two clients that joined at different moments drift apart with no way back.
    // Re-broadcast periodically to pull them together again.
    std::chrono::steady_clock::time_point m_lastResync{};
    bool m_timeSetFromFirstPlayer = false;

    World& m_world;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_joinConnection;
};
