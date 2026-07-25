#include <TiltedOnlinePCH.h>

#include <Services/ImGuiOverlayService.h>
#include <Services/ImguiService.h>
#include <Services/InputService.h>
#include <Services/MagicService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>

#include <World.h>

#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/ConnectionErrorEvent.h>
#include <Events/PartyJoinedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/SetTimeCommandEvent.h>

#include <Messages/NotifyChatMessageBroadcast.h>
#include <Messages/NotifyPartyInfo.h>
#include <Messages/NotifyPartyInvite.h>
#include <Messages/NotifyPlayerCellChanged.h>
#include <Messages/NotifyPlayerDialogue.h>
#include <Messages/NotifyPlayerHealthUpdate.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/NotifyPlayerList.h>
#include <Messages/NotifyPlayerLeft.h>
#include <Messages/NotifyPlayerLevel.h>
#include <Messages/SendChatMessageRequest.h>
#include <Messages/TeleportRequest.h>

#include <ChatMessageTypes.h>
#include <Components.h>

#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>
#include <PlayerCharacter.h>

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
constexpr size_t kMaxChatLines = 200;
constexpr size_t kMaxNotifications = 5;
constexpr size_t kMaxChatMessageLength = 512;
constexpr size_t kMaxServerListBytes = 4 * 1024 * 1024;
constexpr auto kNotificationLifetime = std::chrono::seconds(5);
constexpr auto kRevealCooldown = std::chrono::seconds(10);
constexpr wchar_t kServerListHost[] = L"skyrim-reborn-list.skyrim-together.com";
constexpr wchar_t kServerListPath[] = L"/list";

struct InternetHandle
{
    explicit InternetHandle(HINTERNET aHandle = nullptr) noexcept
        : handle(aHandle)
    {
    }

    ~InternetHandle() noexcept
    {
        if (handle)
            WinHttpCloseHandle(handle);
    }

    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;

    operator HINTERNET() const noexcept { return handle; }
    [[nodiscard]] bool IsValid() const noexcept { return handle != nullptr; }

    HINTERNET handle;
};

std::string ToLower(std::string aText)
{
    std::transform(aText.begin(), aText.end(), aText.begin(), [](unsigned char aCharacter) { return static_cast<char>(std::tolower(aCharacter)); });
    return aText;
}

std::string Trim(std::string aText)
{
    const auto first = std::find_if_not(aText.begin(), aText.end(), [](unsigned char aCharacter) { return std::isspace(aCharacter) != 0; });
    const auto last = std::find_if_not(aText.rbegin(), aText.rend(), [](unsigned char aCharacter) { return std::isspace(aCharacter) != 0; }).base();

    if (first >= last)
        return {};

    return std::string(first, last);
}

std::string NormalizeVersion(std::string aVersion)
{
    aVersion = ToLower(Trim(std::move(aVersion)));
    const auto separator = aVersion.find('-');
    if (separator != std::string::npos)
        aVersion.resize(separator);
    return aVersion;
}

std::filesystem::path GetNativeOverlaySettingsPath()
{
    return TiltedPhoques::GetPath() / "Data" / "SkyrimTogetherReborn" / "native_overlay.json";
}

std::string ResolveCellName(World& aWorld, const GameId& acWorldSpaceId, const GameId& acCellId) noexcept
{
    auto& modSystem = aWorld.GetModSystem();

    if (acWorldSpaceId)
    {
        const uint32_t worldSpaceId = modSystem.GetGameId(acWorldSpaceId);
        if (auto* pWorldSpace = Cast<TESWorldSpace>(TESForm::GetById(worldSpaceId)))
        {
            const char* pName = pWorldSpace->GetName();
            return pName && *pName ? pName : "Unknown";
        }
    }
    else
    {
        const uint32_t cellId = modSystem.GetGameId(acCellId);
        if (auto* pCell = Cast<TESObjectCELL>(TESForm::GetById(cellId)))
        {
            const char* pName = pCell->GetName();
            return pName && *pName ? pName : "Unknown";
        }
    }

    return "Unknown";
}

float CalculateHealthPercentage(Actor* apActor) noexcept
{
    if (!apActor)
        return -1.f;

    const float maxHealth = apActor->GetActorPermanentValue(ActorValueInfo::kHealth);
    const float tempHealth = apActor->healthModifiers.temporaryModifier;
    const float totalHealth = maxHealth + tempHealth;
    if (totalHealth <= 0.f)
        return 0.f;

    return std::clamp(apActor->GetActorValue(ActorValueInfo::kHealth) / totalHealth * 100.f, 0.f, 100.f);
}

std::string WinHttpError(const char* acAction)
{
    return std::string(acAction) + " failed (WinHTTP error " + std::to_string(GetLastError()) + ")";
}

ImVec4 SkyrimAccent(float aAlpha = 1.0f)
{
    return ImVec4(0.78f, 0.67f, 0.42f, aAlpha);
}

void DrawDiamond(ImDrawList* apDrawList, const ImVec2& acCenter, float aRadius, ImU32 aColor)
{
    apDrawList->AddQuadFilled(
        ImVec2(acCenter.x, acCenter.y - aRadius), ImVec2(acCenter.x + aRadius, acCenter.y), ImVec2(acCenter.x, acCenter.y + aRadius), ImVec2(acCenter.x - aRadius, acCenter.y),
        aColor);
}

void DrawSkyrimWindowFrame()
{
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 minimum(windowPos.x + 3.f, windowPos.y + 3.f);
    const ImVec2 maximum(windowPos.x + windowSize.x - 3.f, windowPos.y + windowSize.y - 3.f);
    const ImU32 subtle = ImGui::ColorConvertFloat4ToU32(SkyrimAccent(0.24f));
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(SkyrimAccent(0.68f));
    constexpr float cornerLength = 22.f;

    pDrawList->AddRect(minimum, maximum, subtle, 0.f, 0, 1.f);

    pDrawList->AddLine(minimum, ImVec2(minimum.x + cornerLength, minimum.y), accent, 1.5f);
    pDrawList->AddLine(minimum, ImVec2(minimum.x, minimum.y + cornerLength), accent, 1.5f);
    pDrawList->AddLine(ImVec2(maximum.x - cornerLength, minimum.y), ImVec2(maximum.x, minimum.y), accent, 1.5f);
    pDrawList->AddLine(ImVec2(maximum.x, minimum.y), ImVec2(maximum.x, minimum.y + cornerLength), accent, 1.5f);
    pDrawList->AddLine(ImVec2(minimum.x, maximum.y - cornerLength), ImVec2(minimum.x, maximum.y), accent, 1.5f);
    pDrawList->AddLine(ImVec2(minimum.x, maximum.y), ImVec2(minimum.x + cornerLength, maximum.y), accent, 1.5f);
    pDrawList->AddLine(ImVec2(maximum.x - cornerLength, maximum.y), maximum, accent, 1.5f);
    pDrawList->AddLine(ImVec2(maximum.x, maximum.y - cornerLength), maximum, accent, 1.5f);
}

void DrawSkyrimSectionHeading(const char* acLabel)
{
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 textSize = ImGui::CalcTextSize(acLabel);
    const float width = ImGui::GetContentRegionAvail().x;
    const float textX = cursor.x + std::max(0.f, (width - textSize.x) * 0.5f);
    const float centerY = cursor.y + textSize.y * 0.5f;
    const float ornamentGap = 14.f;
    const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(SkyrimAccent(0.54f));
    const ImU32 textColor = ImGui::ColorConvertFloat4ToU32(SkyrimAccent());
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    const float leftDiamondX = textX - ornamentGap;
    const float rightDiamondX = textX + textSize.x + ornamentGap;

    if (leftDiamondX - cursor.x > 10.f)
    {
        pDrawList->AddLine(ImVec2(cursor.x, centerY), ImVec2(leftDiamondX - 5.f, centerY), lineColor, 1.f);
        DrawDiamond(pDrawList, ImVec2(leftDiamondX, centerY), 3.f, lineColor);
    }

    const float contentEnd = cursor.x + width;
    if (contentEnd - rightDiamondX > 10.f)
    {
        DrawDiamond(pDrawList, ImVec2(rightDiamondX, centerY), 3.f, lineColor);
        pDrawList->AddLine(ImVec2(rightDiamondX + 5.f, centerY), ImVec2(contentEnd, centerY), lineColor, 1.f);
    }

    pDrawList->AddText(ImVec2(textX, cursor.y), textColor, acLabel);
    ImGui::Dummy(ImVec2(width, textSize.y + 8.f));
}

const char* GetChatChannelName(uint8_t aType) noexcept
{
    switch (static_cast<ChatMessageType>(aType))
    {
    case kPartyChat: return "Party";
    case kLocalChat: return "Local";
    default: return "Global";
    }
}

ImVec4 GetChatColor(uint8_t aType) noexcept
{
    switch (static_cast<ChatMessageType>(aType))
    {
    case kPlayerDialogue: return ImVec4(0.80f, 0.76f, 0.64f, 1.f);
    case kPartyChat: return ImVec4(0.55f, 0.78f, 0.58f, 1.f);
    case kLocalChat: return ImVec4(0.66f, 0.72f, 0.78f, 1.f);
    default: return SkyrimAccent();
    }
}

std::string FormatChatTime(const std::chrono::system_clock::time_point& acTimestamp) noexcept
{
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(acTimestamp);
    std::tm localTime{};
    if (localtime_s(&localTime, &timestamp) != 0)
        return "--:--:--";

    char buffer[16]{};
    if (std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime) == 0)
        return "--:--:--";

    return buffer;
}

const char* GetPartyAnchorName(int aAnchor) noexcept
{
    switch (aAnchor)
    {
    case 1: return "Top right";
    case 2: return "Bottom right";
    case 3: return "Bottom left";
    default: return "Top left";
    }
}
} // namespace

ImGuiOverlayService::ImGuiOverlayService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher, ImguiService& aImguiService)
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_drawConnection = aImguiService.OnDraw.connect<&ImGuiOverlayService::OnDraw>(this);
    m_connectedConnection = aDispatcher.sink<ConnectedEvent>().connect<&ImGuiOverlayService::OnConnected>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&ImGuiOverlayService::OnDisconnected>(this);
    m_connectionErrorConnection = aDispatcher.sink<ConnectionErrorEvent>().connect<&ImGuiOverlayService::OnConnectionError>(this);
    m_chatConnection = aDispatcher.sink<NotifyChatMessageBroadcast>().connect<&ImGuiOverlayService::OnChatMessage>(this);
    m_playerDialogueConnection = aDispatcher.sink<NotifyPlayerDialogue>().connect<&ImGuiOverlayService::OnPlayerDialogue>(this);
    m_playerListConnection = aDispatcher.sink<NotifyPlayerList>().connect<&ImGuiOverlayService::OnPlayerList>(this);
    m_playerJoinedConnection = aDispatcher.sink<NotifyPlayerJoined>().connect<&ImGuiOverlayService::OnPlayerJoined>(this);
    m_playerLeftConnection = aDispatcher.sink<NotifyPlayerLeft>().connect<&ImGuiOverlayService::OnPlayerLeft>(this);
    m_playerLevelConnection = aDispatcher.sink<NotifyPlayerLevel>().connect<&ImGuiOverlayService::OnPlayerLevel>(this);
    m_playerCellConnection = aDispatcher.sink<NotifyPlayerCellChanged>().connect<&ImGuiOverlayService::OnPlayerCellChanged>(this);
    m_playerHealthConnection = aDispatcher.sink<NotifyPlayerHealthUpdate>().connect<&ImGuiOverlayService::OnPlayerHealthUpdate>(this);
    m_partyInfoConnection = aDispatcher.sink<NotifyPartyInfo>().connect<&ImGuiOverlayService::OnPartyInfo>(this);
    m_partyInviteConnection = aDispatcher.sink<NotifyPartyInvite>().connect<&ImGuiOverlayService::OnPartyInvite>(this);
    m_partyJoinedConnection = aDispatcher.sink<PartyJoinedEvent>().connect<&ImGuiOverlayService::OnPartyJoined>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&ImGuiOverlayService::OnPartyLeft>(this);
    m_playerAddedConnection = m_world.on_destroy<WaitingFor3D>().connect<&ImGuiOverlayService::OnWaitingFor3DRemoved>(this);
    m_playerRemovedConnection = m_world.on_destroy<PlayerComponent>().connect<&ImGuiOverlayService::OnPlayerComponentRemoved>(this);

    m_statusLine = "Not connected";
    LoadSettings();
    FlashPartyHud();
}

ImGuiOverlayService::~ImGuiOverlayService() noexcept = default;

void ImGuiOverlayService::Toggle() noexcept
{
    SetVisible(!m_visible);
}

void ImGuiOverlayService::SetVisible(bool aVisible) noexcept
{
    const bool wasVisible = m_visible;
    if (m_visible != aVisible)
    {
        m_visible = aVisible;
        spdlog::info("[overlay] native ImGui UI {}", m_visible ? "opened" : "closed");
    }

    if (wasVisible && !m_visible)
    {
        FlashPartyHud();
        SaveSettings();
    }

    // Input remains captured while either the regular overlay or the F3 debug UI
    // is open. Reconcile even if visibility did not change to recover focus under
    // Wine/Proton.
    InputService::RefreshInputState();

    if (m_visible && !m_serverListLoaded && !m_serverListLoading)
        RefreshPublicServers();
}

void ImGuiOverlayService::AddSystemMessage(const std::string& acText) noexcept
{
    AddChatMessage(kSystemMessage, {}, acText);
}

void ImGuiOverlayService::PushSystemMessage(const std::string& acText) noexcept
{
    AddSystemMessage(acText);
}

void ImGuiOverlayService::AddChatMessage(uint8_t aType, const std::string& acAuthor, const std::string& acText) noexcept
{
    const auto now = std::chrono::system_clock::now();
    m_chat.push_back(ChatLine{aType, acAuthor, acText, now});
    while (m_chat.size() > kMaxChatLines)
        m_chat.pop_front();
    m_scrollChatToBottom = true;

    std::string notificationText;
    if (acAuthor.empty())
        notificationText = acText;
    else
    {
        if (aType != kGlobalChat && aType != kPlayerDialogue)
            notificationText = "[" + std::string(GetChatChannelName(aType)) + "] ";
        notificationText += acAuthor + ": " + acText;
    }

    m_notifications.push_back(Notification{aType, std::move(notificationText), std::chrono::steady_clock::now() + kNotificationLifetime});
    while (m_notifications.size() > kMaxNotifications)
        m_notifications.pop_front();
}

void ImGuiOverlayService::FlashPartyHud() noexcept
{
    m_partyHudVisibleUntil = std::chrono::steady_clock::now() + std::chrono::seconds(m_partyHudAutoHideSeconds);
}

void ImGuiOverlayService::OnConnected(const ConnectedEvent&) noexcept
{
    m_connected = true;
    m_connecting = false;
    m_statusLine = "Connected";
    m_errorLine.clear();
    FlashPartyHud();
    AddSystemMessage("Connected to server.");
}

void ImGuiOverlayService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_connecting = false;
    m_statusLine = "Disconnected";
    m_players.clear();
    m_invitedPlayers.clear();
    m_acceptingInvites.clear();

    if (wasConnected)
        AddSystemMessage("Disconnected from server.");
}

void ImGuiOverlayService::OnConnectionError(const ConnectionErrorEvent& acEvent) noexcept
{
    bool isWarning = false;
    const std::string message = DescribeConnectionError(acEvent.ErrorDetail.c_str(), isWarning);

    if (isWarning)
    {
        m_warningLine = message;
        AddSystemMessage("Warning: " + message);
        return;
    }

    m_connected = false;
    m_connecting = false;
    m_statusLine = "Connection failed";
    m_errorLine = message;
    AddSystemMessage("Error: " + message);
}

void ImGuiOverlayService::OnChatMessage(const NotifyChatMessageBroadcast& acMessage) noexcept
{
    AddChatMessage(acMessage.MessageType, acMessage.PlayerName.c_str(), acMessage.ChatMessage.c_str());
}

void ImGuiOverlayService::OnPlayerDialogue(const NotifyPlayerDialogue& acMessage) noexcept
{
    AddChatMessage(kPlayerDialogue, acMessage.Name.c_str(), acMessage.Text.c_str());
}

void ImGuiOverlayService::OnPlayerList(const NotifyPlayerList& acMessage) noexcept
{
    std::unordered_set<uint32_t> currentPlayers;
    currentPlayers.reserve(acMessage.Players.size());

    for (const auto& [playerId, playerName] : acMessage.Players)
    {
        currentPlayers.emplace(playerId);
        auto& player = m_players[playerId];
        player.name = playerName.c_str();
    }

    std::erase_if(m_players, [&currentPlayers](const auto& acEntry) { return !currentPlayers.contains(acEntry.first); });
    std::erase_if(m_invitedPlayers, [&currentPlayers](const auto& acEntry) { return !currentPlayers.contains(acEntry.first); });
}

void ImGuiOverlayService::OnPlayerJoined(const NotifyPlayerJoined& acMessage) noexcept
{
    auto& player = m_players[acMessage.PlayerId];
    player.name = acMessage.Username.c_str();
    player.level = acMessage.Level;
    player.cellName = ResolveCellName(m_world, acMessage.WorldSpaceId, acMessage.CellId);
    AddSystemMessage(std::string(acMessage.Username.c_str()) + " joined.");
}

void ImGuiOverlayService::OnPlayerLeft(const NotifyPlayerLeft& acMessage) noexcept
{
    m_players.erase(acMessage.PlayerId);
    AddSystemMessage(std::string(acMessage.Username.c_str()) + " left.");
}

void ImGuiOverlayService::OnPlayerLevel(const NotifyPlayerLevel& acMessage) noexcept
{
    auto& player = m_players[acMessage.PlayerId];
    player.level = acMessage.NewLevel;
    if (!player.name.empty())
        AddSystemMessage(player.name + " reached level " + std::to_string(acMessage.NewLevel) + ".");
}

void ImGuiOverlayService::OnPlayerCellChanged(const NotifyPlayerCellChanged& acMessage) noexcept
{
    m_players[acMessage.PlayerId].cellName = ResolveCellName(m_world, acMessage.WorldSpaceId, acMessage.CellId);
}

void ImGuiOverlayService::OnPlayerHealthUpdate(const NotifyPlayerHealthUpdate& acMessage) noexcept
{
    m_players[acMessage.PlayerId].health = std::clamp(acMessage.Percentage, 0.f, 100.f);
    FlashPartyHud();
}

void ImGuiOverlayService::OnPartyInfo(const NotifyPartyInfo&) noexcept
{
    FlashPartyHud();
}

void ImGuiOverlayService::OnPartyInvite(const NotifyPartyInvite& acMessage) noexcept
{
    const auto player = m_players.find(acMessage.InviterId);
    const std::string name = player != m_players.end() && !player->second.name.empty() ? player->second.name : "player " + std::to_string(acMessage.InviterId);
    AddSystemMessage("Party invite received from " + name + ". Open F2 > Party to respond.");
}

void ImGuiOverlayService::OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept
{
    m_invitedPlayers.clear();
    m_acceptingInvites.clear();
    FlashPartyHud();
    AddSystemMessage(acEvent.IsLeader ? "Party created. You are the leader." : "Joined party.");
}

void ImGuiOverlayService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    m_invitedPlayers.clear();
    m_acceptingInvites.clear();
    AddSystemMessage("Left party.");
}

void ImGuiOverlayService::OnWaitingFor3DRemoved(entt::registry& aRegistry, entt::entity aEntity) noexcept
{
    const auto* pPlayerComponent = aRegistry.try_get<PlayerComponent>(aEntity);
    const auto* pFormIdComponent = aRegistry.try_get<FormIdComponent>(aEntity);
    if (!pPlayerComponent || !pFormIdComponent)
        return;

    auto player = m_players.find(pPlayerComponent->Id);
    if (player == m_players.end())
        return;

    player->second.loaded = true;
    player->second.health = CalculateHealthPercentage(Cast<Actor>(TESForm::GetById(pFormIdComponent->Id)));
}

void ImGuiOverlayService::OnPlayerComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) noexcept
{
    const auto* pPlayerComponent = aRegistry.try_get<PlayerComponent>(aEntity);
    if (!pPlayerComponent)
        return;

    auto player = m_players.find(pPlayerComponent->Id);
    if (player != m_players.end())
        player->second.loaded = false;
}

void ImGuiOverlayService::OnDraw() noexcept
{
    PollPublicServers();

    const auto now = std::chrono::steady_clock::now();
    while (!m_notifications.empty() && m_notifications.front().expiresAt <= now)
        m_notifications.pop_front();

    if (m_connected)
    {
        auto& partyService = m_world.GetPartyService();
        const bool showPartyHud = m_partyHudEnabled && partyService.IsInParty() && (!m_partyHudAutoHide || m_visible || now < m_partyHudVisibleUntil);
        if (showPartyHud)
            DrawPartyHud();
        else if (!partyService.IsInParty() && !partyService.GetInvitations().empty())
            DrawPartyInviteNotice();

        if (m_networkHudEnabled && !m_visible)
            DrawNetworkHud();
    }

    if (!m_visible)
    {
        DrawNotifications();
        return;
    }

    DrawMainWindow();
    DrawChatWindow();
}

void ImGuiOverlayService::DrawMainWindow() noexcept
{
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const ImVec2 initialSize(std::max(560.f, std::min(900.f, displaySize.x - 40.f)), std::max(420.f, std::min(650.f, displaySize.y - 40.f)));

    ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    if (!ImGui::Begin("SKYRIM TOGETHER##native_overlay", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(m_uiScale);
    DrawSkyrimWindowFrame();
    DrawSkyrimSectionHeading("SKYRIM TOGETHER");

    const ImVec4 statusColor = m_connected ? ImVec4(0.62f, 0.72f, 0.50f, 1.f) : (m_connecting ? SkyrimAccent() : ImVec4(0.58f, 0.57f, 0.53f, 1.f));
    ImGui::TextColored(statusColor, "%s", m_statusLine.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  |  Protocol %s  |  Build %s  |  F2 menu  |  F3 debug  |  Esc close", PROTOCOL_VERSION, BUILD_COMMIT);

    if (!m_warningLine.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.78f, 0.28f, 1.f));
        ImGui::TextWrapped("Warning: %s", m_warningLine.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss##warning"))
            m_warningLine.clear();
    }

    if (!m_errorLine.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
        ImGui::TextWrapped("Error: %s", m_errorLine.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss##error"))
            m_errorLine.clear();
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("main_tabs"))
    {
        if (ImGui::BeginTabItem("CONNECT"))
        {
            DrawConnectionTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("PUBLIC SERVERS"))
        {
            DrawServerBrowserTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("PLAYERS"))
        {
            DrawPlayersTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("PARTY"))
        {
            DrawPartyTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("SETTINGS"))
        {
            DrawSettingsTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ImGuiOverlayService::DrawConnectionTab() noexcept
{
    ImGui::Spacing();
    DrawSkyrimSectionHeading("DIRECT CONNECTION");
    ImGui::TextWrapped("Connect directly to a private or self-hosted server. Use the Public servers tab to browse announced servers.");
    ImGui::Spacing();

    ImGui::BeginDisabled(m_connected || m_connecting);
    ImGui::TextDisabled("ADDRESS");
    ImGui::SetNextItemWidth(std::min(460.f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##manual_address", "Address or hostname", m_addressBuffer, std::size(m_addressBuffer));

    ImGui::TextDisabled("PORT");
    ImGui::SetNextItemWidth(150.f);
    ImGui::InputInt("##manual_port", &m_port);

    ImGui::TextDisabled("PASSWORD");
    ImGui::SetNextItemWidth(std::min(460.f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##manual_password", "Password (optional)", m_passwordBuffer, std::size(m_passwordBuffer), ImGuiInputTextFlags_Password);
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (!m_connected && !m_connecting)
    {
        if (ImGui::Button("CONNECT", ImVec2(140.f, 0.f)))
        {
            const int port = std::clamp(m_port, 1, 65535);
            m_port = port;
            Connect(m_addressBuffer, static_cast<uint16_t>(port), m_passwordBuffer);
        }
    }
    else if (m_connecting && ImGui::Button("CANCEL", ImVec2(140.f, 0.f)))
    {
        spdlog::info("[overlay] connection attempt cancelled by user");
        World& world = m_world;
        world.GetRunner().Queue([&world] { world.GetTransport().Close(); });
    }
    else if (m_connected && ImGui::Button("DISCONNECT", ImVec2(140.f, 0.f)))
    {
        // A modal prevents the click that opened the overlay from accidentally
        // disconnecting an active session.
        ImGui::OpenPopup("DISCONNECT FROM SERVER?##confirm_disconnect");
    }

    ImGui::SetNextWindowSize(ImVec2(430.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("DISCONNECT FROM SERVER?##confirm_disconnect", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!m_connected)
        {
            ImGui::CloseCurrentPopup();
        }
        else
        {
            ImGui::TextWrapped("Your current multiplayer session will end. Do you want to disconnect?");
            ImGui::Spacing();
            if (ImGui::Button("KEEP PLAYING", ImVec2(150.f, 0.f)))
                ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("DISCONNECT##confirm", ImVec2(150.f, 0.f)))
            {
                spdlog::info("[overlay] disconnect requested by user");
                World& world = m_world;
                world.GetRunner().Queue([&world] { world.GetTransport().Close(); });
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void ImGuiOverlayService::DrawServerBrowserTab() noexcept
{
    ImGui::Spacing();
    DrawSkyrimSectionHeading("SERVER BROWSER");

    ImGui::BeginDisabled(m_serverListLoading);
    if (ImGui::Button("REFRESH"))
        RefreshPublicServers();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (m_serverListLoading)
        ImGui::TextDisabled("Loading public servers...");
    else
        ImGui::TextDisabled("%zu servers received", m_publicServers.size());

    ImGui::TextDisabled("SEARCH");
    ImGui::SetNextItemWidth(std::min(360.f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##server_search", "Search by name or description", m_serverSearchBuffer, std::size(m_serverSearchBuffer));

    bool filtersChanged = false;
    filtersChanged |= ImGui::Checkbox("HIDE FULL", &m_hideFullServers);
    ImGui::SameLine();
    filtersChanged |= ImGui::Checkbox("HIDE PASSWORD PROTECTED", &m_hidePasswordServers);
    ImGui::SameLine();
    filtersChanged |= ImGui::Checkbox("HIDE VERSION MISMATCH", &m_hideVersionMismatch);
    if (filtersChanged)
        SaveSettings();

    if (!m_serverListError.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
        ImGui::TextWrapped("%s", m_serverListError.c_str());
        ImGui::PopStyleColor();
    }

    const std::string search = ToLower(m_serverSearchBuffer);
    std::vector<const PublicServer*> visibleServers;
    visibleServers.reserve(m_publicServers.size());

    for (const auto& server : m_publicServers)
    {
        if (m_hideFullServers && server.maxPlayerCount > 0 && server.playerCount >= server.maxPlayerCount)
            continue;
        if (m_hidePasswordServers && server.passwordProtected)
            continue;
        if (m_hideVersionMismatch && !IsVersionCompatible(server))
            continue;

        if (!search.empty())
        {
            const std::string searchableText = ToLower(server.name + " " + server.description + " " + server.address);
            if (searchableText.find(search) == std::string::npos)
                continue;
        }

        visibleServers.push_back(&server);
    }

    std::stable_sort(
        visibleServers.begin(), visibleServers.end(),
        [this](const PublicServer* apLeft, const PublicServer* apRight)
        {
            const bool leftFavorite = m_favoriteServers.contains(MakeServerKey(*apLeft));
            const bool rightFavorite = m_favoriteServers.contains(MakeServerKey(*apRight));
            if (leftFavorite != rightFavorite)
                return leftFavorite;
            if (apLeft->playerCount != apRight->playerCount)
                return apLeft->playerCount > apRight->playerCount;
            return ToLower(apLeft->name) < ToLower(apRight->name);
        });

    ImGui::TextDisabled("%zu shown", visibleServers.size());

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    const float tableHeight = std::max(180.f, ImGui::GetContentRegionAvail().y - 125.f);

    if (ImGui::BeginTable("public_server_table", 5, tableFlags, ImVec2(0.f, tableHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("FAV", ImGuiTableColumnFlags_WidthFixed, 42.f);
        ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch, 0.52f);
        ImGui::TableSetupColumn("PLAYERS", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("VERSION", ImGuiTableColumnFlags_WidthFixed, 90.f);
        ImGui::TableSetupColumn("ACCESS", ImGuiTableColumnFlags_WidthFixed, 88.f);
        ImGui::TableHeadersRow();

        for (const PublicServer* pServer : visibleServers)
        {
            const std::string key = MakeServerKey(*pServer);
            const bool favorite = m_favoriteServers.contains(key);
            const bool selected = m_selectedServerKey == key;

            ImGui::PushID(key.c_str());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::SmallButton(favorite ? "*" : "+"))
                ToggleFavorite(*pServer);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(favorite ? "Remove favorite" : "Add favorite");

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable(pServer->name.c_str(), selected))
            {
                m_selectedServerKey = key;
                m_serverPasswordBuffer[0] = '\0';

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !pServer->passwordProtected && !m_connected && !m_connecting)
                    Connect(pServer->address, pServer->port, {});
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u/%u", pServer->playerCount, pServer->maxPlayerCount);

            ImGui::TableSetColumnIndex(3);
            if (!IsVersionCompatible(*pServer))
                ImGui::TextColored(ImVec4(1.f, 0.55f, 0.3f, 1.f), "%s", pServer->version.c_str());
            else
                ImGui::TextUnformatted(pServer->version.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(pServer->passwordProtected ? "PASSWORD" : "OPEN");
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    const PublicServer* pSelectedServer = nullptr;
    for (const auto& server : m_publicServers)
    {
        if (MakeServerKey(server) == m_selectedServerKey)
        {
            pSelectedServer = &server;
            break;
        }
    }

    if (pSelectedServer)
    {
        DrawSkyrimSectionHeading("SELECTED SERVER");
        ImGui::TextColored(SkyrimAccent(), "%s", pSelectedServer->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", MakeEndpoint(pSelectedServer->address, pSelectedServer->port).c_str());

        if (!pSelectedServer->description.empty())
            ImGui::TextWrapped("%s", pSelectedServer->description.c_str());

        if (pSelectedServer->passwordProtected)
        {
            ImGui::SetNextItemWidth(260.f);
            ImGui::InputTextWithHint("##public_server_password", "Server password", m_serverPasswordBuffer, std::size(m_serverPasswordBuffer), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
        }

        ImGui::BeginDisabled(m_connected || m_connecting);
        if (ImGui::Button("CONNECT TO SELECTED"))
            Connect(pSelectedServer->address, pSelectedServer->port, m_serverPasswordBuffer);
        ImGui::EndDisabled();
    }
}

void ImGuiOverlayService::DrawPlayersTab() noexcept
{
    ImGui::Spacing();
    DrawSkyrimSectionHeading("ADVENTURERS");
    ImGui::Text("%zu other players online", m_players.size());
    ImGui::SameLine();
    const auto now = std::chrono::steady_clock::now();
    const bool revealOnCooldown = now < m_revealAvailableAt;
    const auto revealSecondsRemaining = revealOnCooldown ? std::chrono::duration_cast<std::chrono::seconds>(m_revealAvailableAt - now + std::chrono::milliseconds(999)).count() : 0;
    const std::string revealLabel = revealOnCooldown ? "REVEAL (" + std::to_string(revealSecondsRemaining) + "s)" : "REVEAL PLAYERS";

    ImGui::BeginDisabled(!m_connected || revealOnCooldown);
    if (ImGui::Button(revealLabel.c_str()))
    {
        m_revealAvailableAt = now + kRevealCooldown;
        AddSystemMessage("Revealing nearby players for 10 seconds.");
        m_world.GetMagicService().StartRevealingOtherPlayers();
        SetVisible(false);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(revealOnCooldown ? "Reveal Players is still active." : "Temporarily reveals nearby player markers, then closes the menu.");
    ImGui::Separator();

    if (m_players.empty())
    {
        ImGui::TextDisabled(m_connected ? "No other players are currently visible." : "Connect to a server to see its players.");
        return;
    }

    auto& partyService = m_world.GetPartyService();
    const auto& partyMembers = partyService.GetPartyMembers();
    const uint64_t currentTick = m_transport.GetClock().GetCurrentTick();
    std::erase_if(m_invitedPlayers, [currentTick](const auto& acEntry) { return acEntry.second <= currentTick; });

    if (ImGui::BeginTable("players_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("PLAYER", ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn("LEVEL", ImGuiTableColumnFlags_WidthFixed, 90.f);
        ImGui::TableSetupColumn("LOCATION", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn("PARTY", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableHeadersRow();

        for (const auto& [id, player] : m_players)
        {
            const bool isPartyMember = std::find(partyMembers.begin(), partyMembers.end(), id) != partyMembers.end();
            if (isPartyMember)
                m_invitedPlayers.erase(id);

            ImGui::PushID(static_cast<int>(id));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(player.name.c_str());
            ImGui::TableSetColumnIndex(1);
            if (player.level > 0)
                ImGui::Text("%u", player.level);
            else
                ImGui::TextDisabled("?");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(player.cellName.empty() ? "Unknown" : player.cellName.c_str());
            ImGui::TableSetColumnIndex(3);

            if (isPartyMember)
                ImGui::TextColored(SkyrimAccent(), "MEMBER");
            else if (partyService.IsInParty() && partyService.IsLeader())
            {
                const bool invited = m_invitedPlayers.contains(id);
                ImGui::BeginDisabled(invited);
                if (ImGui::SmallButton(invited ? "INVITED" : "INVITE"))
                {
                    partyService.CreateInvite(id);
                    m_invitedPlayers[id] = currentTick + 60000;
                    AddSystemMessage("Party invite sent to " + player.name + ".");
                }
                ImGui::EndDisabled();
            }
            else
                ImGui::TextDisabled("-");

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void ImGuiOverlayService::DrawPartyTab() noexcept
{
    ImGui::Spacing();
    DrawSkyrimSectionHeading("PARTY");

    if (!m_connected)
    {
        ImGui::TextDisabled("Connect to a server to create or join a party.");
        return;
    }

    auto& partyService = m_world.GetPartyService();
    auto& invitations = partyService.GetInvitations();

    const auto getPlayerName = [this, &partyService](uint32_t aPlayerId)
    {
        if (aPlayerId == m_transport.GetLocalPlayerId())
            return std::string("You");

        if (const auto player = m_players.find(aPlayerId); player != m_players.end() && !player->second.name.empty())
            return player->second.name;

        if (const auto player = partyService.GetPlayers().find(aPlayerId); player != partyService.GetPlayers().end())
            return std::string(player->second.c_str());

        return std::string("Player ") + std::to_string(aPlayerId);
    };

    if (!partyService.IsInParty())
    {
        ImGui::TextWrapped("Party members share quests and world progression. The leader should handle important conversations, quest items, and major world interactions.");
        ImGui::Spacing();

        if (ImGui::Button("CREATE PARTY", ImVec2(160.f, 0.f)))
            partyService.CreateParty();

        ImGui::Spacing();
        DrawSkyrimSectionHeading("PENDING INVITES");

        if (invitations.empty())
        {
            ImGui::TextDisabled("No pending party invitations.");
            return;
        }

        const uint64_t currentTick = m_transport.GetClock().GetCurrentTick();
        for (const auto& [inviterId, expiryTick] : invitations)
        {
            ImGui::PushID(static_cast<int>(inviterId));
            const std::string name = getPlayerName(inviterId);
            const uint64_t remainingSeconds = expiryTick > currentTick ? (expiryTick - currentTick + 999) / 1000 : 0;

            ImGui::TextColored(SkyrimAccent(), "%s", name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("expires in %llu s", static_cast<unsigned long long>(remainingSeconds));
            ImGui::SameLine();

            const bool accepting = m_acceptingInvites.contains(inviterId);
            ImGui::BeginDisabled(accepting);
            if (ImGui::SmallButton(accepting ? "ACCEPTING..." : "ACCEPT"))
            {
                m_acceptingInvites.emplace(inviterId);
                World* pWorld = &m_world;
                pWorld->GetRunner().Queue(
                    [pWorld, inviterId]
                    {
                        auto& service = pWorld->GetPartyService();
                        service.AcceptInvite(inviterId);
                        service.GetInvitations().erase(inviterId);
                    });
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }

        return;
    }

    const uint32_t localPlayerId = m_transport.GetLocalPlayerId();
    const auto& partyMembers = partyService.GetPartyMembers();

    if (partyService.IsLeader())
        ImGui::TextColored(SkyrimAccent(), "YOU ARE THE PARTY LEADER");
    else
        ImGui::TextColored(SkyrimAccent(), "PARTY LEADER: %s", getPlayerName(partyService.GetLeaderPlayerId()).c_str());
    ImGui::TextDisabled("%zu members", partyMembers.size());
    ImGui::Separator();

    if (ImGui::BeginTable("party_members_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("PLAYER", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("LEVEL", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("LOCATION", ImGuiTableColumnFlags_WidthStretch, 0.32f);
        ImGui::TableSetupColumn("HEALTH", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("ACTIONS", ImGuiTableColumnFlags_WidthFixed, partyService.IsLeader() ? 270.f : 90.f);
        ImGui::TableHeadersRow();

        for (uint32_t playerId : partyMembers)
        {
            const bool isLocalPlayer = playerId == localPlayerId;
            const bool isLeader = playerId == partyService.GetLeaderPlayerId();
            const auto player = m_players.find(playerId);

            ImGui::PushID(static_cast<int>(playerId));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(isLeader ? SkyrimAccent() : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s%s", getPlayerName(playerId).c_str(), isLeader ? "  [LEADER]" : "");

            ImGui::TableSetColumnIndex(1);
            if (!isLocalPlayer && player != m_players.end() && player->second.level > 0)
                ImGui::Text("%u", player->second.level);
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(2);
            if (!isLocalPlayer && player != m_players.end() && !player->second.cellName.empty())
                ImGui::TextUnformatted(player->second.cellName.c_str());
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(3);
            if (isLocalPlayer)
            {
                const float health = CalculateHealthPercentage(PlayerCharacter::Get());
                ImGui::Text("%.0f%%", health);
            }
            else if (player != m_players.end() && player->second.health >= 0.f)
                ImGui::Text("%.0f%%", player->second.health);
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(4);
            if (!isLocalPlayer)
            {
                if (ImGui::SmallButton("TELEPORT"))
                {
                    TeleportRequest request{};
                    request.PlayerId = static_cast<uint16_t>(playerId);
                    m_transport.Send(request);
                    AddSystemMessage("Teleport requested to " + getPlayerName(playerId) + ".");
                }

                if (partyService.IsLeader())
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("KICK"))
                        partyService.KickPartyMember(playerId);

                    ImGui::SameLine();
                    if (ImGui::SmallButton("MAKE LEADER"))
                        partyService.ChangePartyLeader(playerId);
                }
            }
            else
                ImGui::TextDisabled("YOU");

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (partyService.IsLeader())
        ImGui::TextDisabled("Invite other adventurers from the Players tab.");

    ImGui::Spacing();
    if (ImGui::Button("LEAVE PARTY", ImVec2(160.f, 0.f)))
        partyService.LeaveParty();
}

void ImGuiOverlayService::DrawSettingsTab() noexcept
{
    ImGui::Spacing();
    DrawSkyrimSectionHeading("NATIVE UI SETTINGS");

    bool saveSettings = false;

    DrawSkyrimSectionHeading("INTERFACE");
    ImGui::TextDisabled("UI SCALE");
    ImGui::SetNextItemWidth(280.f);
    ImGui::SliderFloat("##native_ui_scale", &m_uiScale, 0.80f, 1.50f, "%.2fx");
    saveSettings |= ImGui::IsItemDeactivatedAfterEdit();

    saveSettings |= ImGui::Checkbox("SHOW COMPACT NETWORK STATS", &m_networkHudEnabled);

    bool debugVisible = m_world.GetDebugService().IsVisible();
    if (ImGui::Checkbox("SHOW DEBUG TOOLS (F3)", &debugVisible))
        m_world.GetDebugService().SetVisible(debugVisible);

    ImGui::Spacing();
    DrawSkyrimSectionHeading("PARTY HUD");

    if (ImGui::Checkbox("SHOW PARTY HUD", &m_partyHudEnabled))
    {
        saveSettings = true;
        FlashPartyHud();
    }

    ImGui::BeginDisabled(!m_partyHudEnabled);
    if (ImGui::Checkbox("AUTO-HIDE PARTY HUD", &m_partyHudAutoHide))
    {
        saveSettings = true;
        FlashPartyHud();
    }

    ImGui::BeginDisabled(!m_partyHudAutoHide);
    ImGui::TextDisabled("AUTO-HIDE DELAY");
    ImGui::SetNextItemWidth(180.f);
    const std::string autoHidePreview = std::to_string(m_partyHudAutoHideSeconds) + " second" + (m_partyHudAutoHideSeconds == 1 ? "" : "s");
    if (ImGui::BeginCombo("##party_auto_hide_delay", autoHidePreview.c_str()))
    {
        constexpr int delays[] = {1, 3, 5};
        for (int delay : delays)
        {
            const bool selected = delay == m_partyHudAutoHideSeconds;
            const std::string label = std::to_string(delay) + " second" + (delay == 1 ? "" : "s");
            if (ImGui::Selectable(label.c_str(), selected))
            {
                m_partyHudAutoHideSeconds = delay;
                saveSettings = true;
                FlashPartyHud();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::TextDisabled("ANCHOR");
    ImGui::SetNextItemWidth(180.f);
    if (ImGui::BeginCombo("##party_hud_anchor", GetPartyAnchorName(m_partyHudAnchor)))
    {
        for (int anchor = 0; anchor < 4; ++anchor)
        {
            const bool selected = anchor == m_partyHudAnchor;
            if (ImGui::Selectable(GetPartyAnchorName(anchor), selected))
            {
                m_partyHudAnchor = anchor;
                saveSettings = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::TextDisabled("HORIZONTAL OFFSET FROM EDGE");
    ImGui::SetNextItemWidth(280.f);
    ImGui::SliderFloat("##party_hud_offset_x", &m_partyHudOffsetX, 0.f, 84.f, "%.0f%%");
    saveSettings |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::TextDisabled("VERTICAL OFFSET FROM EDGE");
    ImGui::SetNextItemWidth(280.f);
    ImGui::SliderFloat("##party_hud_offset_y", &m_partyHudOffsetY, 2.f, 50.f, "%.0f%%");
    saveSettings |= ImGui::IsItemDeactivatedAfterEdit();

    if (ImGui::Button("RESET PARTY HUD POSITION"))
    {
        m_partyHudAnchor = 0;
        m_partyHudOffsetX = 2.f;
        m_partyHudOffsetY = 8.f;
        saveSettings = true;
        FlashPartyHud();
    }
    ImGui::EndDisabled();

    if (saveSettings)
        SaveSettings();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextWrapped("Chat commands: /help, /global, /local, /party, and /settime <hour> <minute>.");
    ImGui::TextDisabled("Use Up/Down in the chat input to browse message history. Native UI preferences are stored next to the mod data.");
}

void ImGuiOverlayService::DrawPartyHud() noexcept
{
    auto& partyService = m_world.GetPartyService();
    const auto& partyMembers = partyService.GetPartyMembers();
    if (partyMembers.empty())
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float xOffset = displaySize.x * m_partyHudOffsetX / 100.f;
    const float yOffset = displaySize.y * m_partyHudOffsetY / 100.f;
    ImVec2 position;
    ImVec2 pivot;

    switch (m_partyHudAnchor)
    {
    case 1:
        position = ImVec2(displaySize.x - xOffset, yOffset);
        pivot = ImVec2(1.f, 0.f);
        break;
    case 2:
        position = ImVec2(displaySize.x - xOffset, displaySize.y - yOffset);
        pivot = ImVec2(1.f, 1.f);
        break;
    case 3:
        position = ImVec2(xOffset, displaySize.y - yOffset);
        pivot = ImVec2(0.f, 1.f);
        break;
    default:
        position = ImVec2(xOffset, yOffset);
        pivot = ImVec2(0.f, 0.f);
        break;
    }

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.78f);

    if (!ImGui::Begin("PARTY##native_party_hud", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(m_uiScale);
    DrawSkyrimWindowFrame();

    const uint32_t localPlayerId = m_transport.GetLocalPlayerId();
    for (uint32_t playerId : partyMembers)
    {
        const bool isLocalPlayer = playerId == localPlayerId;
        const bool isLeader = playerId == partyService.GetLeaderPlayerId();
        const auto player = m_players.find(playerId);

        std::string name;
        if (isLocalPlayer)
            name = "You";
        else if (player != m_players.end() && !player->second.name.empty())
            name = player->second.name;
        else if (const auto knownPlayer = partyService.GetPlayers().find(playerId); knownPlayer != partyService.GetPlayers().end())
            name = knownPlayer->second.c_str();
        else
            name = "Player " + std::to_string(playerId);

        if (isLeader)
            name += "  [LEADER]";

        ImGui::PushID(static_cast<int>(playerId));
        ImGui::TextColored(isLeader ? SkyrimAccent() : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", name.c_str());

        float health = -1.f;
        if (isLocalPlayer)
            health = CalculateHealthPercentage(PlayerCharacter::Get());
        else if (player != m_players.end())
            health = player->second.health;

        if (health >= 0.f)
        {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.52f, 0.15f, 0.12f, 1.f));
            ImGui::ProgressBar(std::clamp(health / 100.f, 0.f, 1.f), ImVec2(210.f * m_uiScale, 8.f * m_uiScale), "");
            ImGui::PopStyleColor();
        }

        if (!isLocalPlayer && player != m_players.end() && !player->second.cellName.empty())
            ImGui::TextDisabled("%s%s", player->second.loaded ? "" : "Distant - ", player->second.cellName.c_str());

        ImGui::PopID();
    }

    ImGui::End();
}

void ImGuiOverlayService::DrawPartyInviteNotice() noexcept
{
    auto& partyService = m_world.GetPartyService();
    const auto& invitations = partyService.GetInvitations();
    if (invitations.empty())
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 28.f, 82.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowBgAlpha(0.84f);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##native_party_invite_notice", nullptr, flags))
    {
        ImGui::SetWindowFontScale(m_uiScale);
        ImGui::TextColored(SkyrimAccent(), "PARTY INVITATION");

        const uint32_t inviterId = invitations.begin()->first;
        const auto player = m_players.find(inviterId);
        const std::string name = player != m_players.end() && !player->second.name.empty() ? player->second.name : "Player " + std::to_string(inviterId);
        ImGui::Text("%s invited you.", name.c_str());
        ImGui::TextDisabled("Open F2 > Party to respond.");
    }
    ImGui::End();
}

void ImGuiOverlayService::DrawNetworkHud() noexcept
{
    const auto status = m_transport.GetConnectionStatus();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float packetLoss = status.m_flConnectionQualityLocal >= 0.f ? (1.f - std::clamp(status.m_flConnectionQualityLocal, 0.f, 1.f)) * 100.f : 0.f;

    ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, 24.f), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::SetNextWindowBgAlpha(0.72f);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##native_network_hud", nullptr, flags))
    {
        ImGui::SetWindowFontScale(m_uiScale);
        ImGui::TextColored(SkyrimAccent(), "%d ms  %.1f%% loss", status.m_nPing, packetLoss);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("DOWN %.1f kB/s  %.1f p/s", status.m_flInBytesPerSec / 1024.f, status.m_flInPacketsPerSec);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("UP %.1f kB/s  %.1f p/s", status.m_flOutBytesPerSec / 1024.f, status.m_flOutPacketsPerSec);
    }
    ImGui::End();
}

void ImGuiOverlayService::DrawNotifications() noexcept
{
    if (m_notifications.empty())
        return;

    bool partyInviteVisible = false;
    if (m_connected)
    {
        auto& partyService = m_world.GetPartyService();
        partyInviteVisible = !partyService.IsInParty() && !partyService.GetInvitations().empty();
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float maximumWidth = std::max(280.f, std::min(520.f, displaySize.x - 56.f));
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 28.f, partyInviteVisible ? 180.f : 82.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(280.f, 0.f), ImVec2(maximumWidth, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.80f);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##native_notifications", nullptr, flags))
    {
        ImGui::SetWindowFontScale(m_uiScale);
        DrawSkyrimWindowFrame();

        for (size_t i = 0; i < m_notifications.size(); ++i)
        {
            const auto& notification = m_notifications[i];
            ImGui::PushStyleColor(ImGuiCol_Text, notification.type == kSystemMessage ? SkyrimAccent(0.90f) : GetChatColor(notification.type));
            ImGui::TextWrapped("%s", notification.text.c_str());
            ImGui::PopStyleColor();

            if (i + 1 < m_notifications.size())
                ImGui::Separator();
        }
    }
    ImGui::End();
}

void ImGuiOverlayService::DrawChatWindow() noexcept
{
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const ImVec2 initialSize(std::max(360.f, std::min(480.f, displaySize.x - 60.f)), std::max(220.f, std::min(320.f, displaySize.y - 80.f)));

    ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(30.f, std::max(30.f, displaySize.y - initialSize.y - 30.f)), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("CHAT##native_chat"))
    {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(m_uiScale);
    DrawSkyrimWindowFrame();
    DrawSkyrimSectionHeading("CHAT");

    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("chat_scroll", ImVec2(0, -footer), true))
    {
        if (m_chat.empty())
            ImGui::TextDisabled("Chat messages and system notifications will appear here.");

        for (const auto& line : m_chat)
        {
            const std::string timestamp = FormatChatTime(line.timestamp);
            ImGui::TextDisabled("[%s]", timestamp.c_str());
            ImGui::SameLine();

            if (line.type == kSystemMessage || line.author.empty())
                ImGui::TextColored(SkyrimAccent(0.72f), "%s", line.text.c_str());
            else
            {
                if (line.type != kGlobalChat && line.type != kPlayerDialogue)
                {
                    ImGui::TextColored(GetChatColor(line.type), "[%s] ", GetChatChannelName(line.type));
                    ImGui::SameLine(0.f, 0.f);
                }

                ImGui::TextColored(GetChatColor(line.type), "%s", line.author.c_str());
                ImGui::SameLine(0.f, 0.f);
                ImGui::TextWrapped(": %s", line.text.c_str());
            }
        }

        if (m_scrollChatToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollChatToBottom = false;
        }
    }
    ImGui::EndChild();

    if (!m_connected)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.36f, 1.f), "CHAT UNAVAILABLE - NOT CONNECTED");
    }

    ImGui::BeginDisabled(!m_connected);
    ImGui::SetNextItemWidth(82.f);
    if (ImGui::BeginCombo("##chat_channel", GetChatChannelName(static_cast<uint8_t>(m_chatChannel))))
    {
        constexpr ChatMessageType channels[] = {kGlobalChat, kPartyChat, kLocalChat};
        for (ChatMessageType channel : channels)
        {
            const bool selected = m_chatChannel == channel;
            const bool available = channel != kPartyChat || m_world.GetPartyService().IsInParty();
            ImGui::BeginDisabled(!available);
            if (ImGui::Selectable(GetChatChannelName(channel), selected))
                m_chatChannel = channel;
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-90.f);
    const bool submitted = ImGui::InputTextWithHint(
        "##chat_input", m_connected ? "Message or /help" : "Connect to a server to chat", m_chatInputBuffer, std::size(m_chatInputBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, &ImGuiOverlayService::ChatHistoryCallback, this);
    ImGui::SameLine();
    const bool sendClicked = ImGui::Button("SEND");
    ImGui::EndDisabled();

    if ((submitted || sendClicked) && m_chatInputBuffer[0] != '\0')
        SubmitChatInput();

    ImGui::End();
}

void ImGuiOverlayService::SubmitChatInput() noexcept
{
    std::string input = Trim(m_chatInputBuffer);
    m_chatInputBuffer[0] = '\0';

    if (input.empty())
        return;

    if (m_chatHistory.empty() || m_chatHistory.back() != input)
    {
        m_chatHistory.emplace_back(input);
        if (m_chatHistory.size() > 50)
            m_chatHistory.erase(m_chatHistory.begin());
    }
    m_chatHistoryIndex = -1;

    if (!input.starts_with('/'))
    {
        SendChatMessage(static_cast<uint8_t>(m_chatChannel), input);
        return;
    }

    std::istringstream commandStream(input.substr(1));
    std::string command;
    commandStream >> command;
    command = ToLower(command);

    if (command == "help")
    {
        AddSystemMessage("Commands: /global <message>, /local <message>, /party <message>, /settime <hour> <minute>.");
        return;
    }

    if (command == "settime")
    {
        int hours = -1;
        int minutes = -1;
        std::string trailing;
        if (!(commandStream >> hours >> minutes) || (commandStream >> trailing) || hours < 0 || hours > 23 || minutes < 0 || minutes > 59)
        {
            AddSystemMessage("Usage: /settime <hour 0-23> <minute 0-59>.");
            return;
        }

        m_world.GetDispatcher().trigger(SetTimeCommandEvent(static_cast<uint8_t>(hours), static_cast<uint8_t>(minutes), m_transport.GetLocalPlayerId()));
        AddSystemMessage("Requested server time " + std::to_string(hours) + ":" + (minutes < 10 ? "0" : "") + std::to_string(minutes) + ".");
        return;
    }

    ChatMessageType messageType = kGlobalChat;
    if (command == "party")
        messageType = kPartyChat;
    else if (command == "local")
        messageType = kLocalChat;
    else if (command != "global")
    {
        AddSystemMessage("Unknown command '/" + command + "'. Type /help for available commands.");
        return;
    }

    if (messageType == kPartyChat && !m_world.GetPartyService().IsInParty())
    {
        AddSystemMessage("Join a party before using party chat.");
        return;
    }

    std::string contents;
    std::getline(commandStream >> std::ws, contents);
    contents = Trim(std::move(contents));
    if (contents.empty())
    {
        AddSystemMessage("Enter a message after /" + command + ".");
        return;
    }

    SendChatMessage(messageType, contents);
}

void ImGuiOverlayService::SendChatMessage(uint8_t aType, const std::string& acText) noexcept
{
    if (!m_connected)
    {
        AddSystemMessage("Connect to a server before sending chat messages.");
        return;
    }

    if (aType == kPartyChat && !m_world.GetPartyService().IsInParty())
    {
        AddSystemMessage("Join a party before using party chat.");
        return;
    }

    if (acText.size() >= kMaxChatMessageLength)
    {
        AddSystemMessage("Chat messages must be shorter than " + std::to_string(kMaxChatMessageLength) + " characters.");
        return;
    }

    SendChatMessageRequest request;
    request.MessageType = static_cast<ChatMessageType>(aType);
    request.ChatMessage = acText;
    m_transport.Send(request);
}

int ImGuiOverlayService::ChatHistoryCallback(ImGuiInputTextCallbackData* apData) noexcept
{
    auto* pOverlay = static_cast<ImGuiOverlayService*>(apData->UserData);
    if (!pOverlay || apData->EventFlag != ImGuiInputTextFlags_CallbackHistory || pOverlay->m_chatHistory.empty())
        return 0;

    if (apData->EventKey == ImGuiKey_UpArrow)
    {
        if (pOverlay->m_chatHistoryIndex < 0)
            pOverlay->m_chatHistoryIndex = static_cast<int>(pOverlay->m_chatHistory.size()) - 1;
        else if (pOverlay->m_chatHistoryIndex > 0)
            --pOverlay->m_chatHistoryIndex;
    }
    else if (apData->EventKey == ImGuiKey_DownArrow)
    {
        if (pOverlay->m_chatHistoryIndex >= 0 && ++pOverlay->m_chatHistoryIndex >= static_cast<int>(pOverlay->m_chatHistory.size()))
            pOverlay->m_chatHistoryIndex = -1;
    }

    const char* pHistoryEntry = pOverlay->m_chatHistoryIndex >= 0 ? pOverlay->m_chatHistory[pOverlay->m_chatHistoryIndex].c_str() : "";
    apData->DeleteChars(0, apData->BufTextLen);
    apData->InsertChars(0, pHistoryEntry);
    return 0;
}

void ImGuiOverlayService::Connect(const std::string& acAddress, uint16_t aPort, const std::string& acPassword) noexcept
{
    std::string address = Trim(acAddress);
    if (address == "localhost")
        address = "127.0.0.1";

    if (address.empty())
    {
        m_errorLine = "Enter a valid server address.";
        return;
    }

    const std::string endpoint = MakeEndpoint(address, aPort);
    const bool directIp = address.find(':') != std::string::npos;
    m_transport.SetServerPassword(acPassword);
    m_connecting = true;
    m_statusLine = "Connecting to " + endpoint + "...";
    m_errorLine.clear();
    m_warningLine.clear();

    spdlog::info("[overlay] connecting to {} via {} with protocol {} (build {})", endpoint, directIp ? "direct IP" : "name resolution", PROTOCOL_VERSION, BUILD_COMMIT);
    World& world = m_world;
    world.GetRunner().Queue(
        [&world, endpoint, directIp]
        {
            const bool started = directIp ? world.GetTransport().ConnectByIp(endpoint) : world.GetTransport().Connect(endpoint);
            if (!started)
                world.GetTransport().OnDisconnected(Client::kLocalProblem);
        });
}

ImGuiOverlayService::ServerListResult ImGuiOverlayService::FetchPublicServers() noexcept
{
    ServerListResult result;

    try
    {
        InternetHandle session(WinHttpOpen(L"SkyrimTogetherNativeUI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.IsValid())
        {
            result.error = WinHttpError("Opening the HTTP session");
            return result;
        }

        WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);

        InternetHandle connection(WinHttpConnect(session, kServerListHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.IsValid())
        {
            result.error = WinHttpError("Connecting to the public server service");
            return result;
        }

        const wchar_t* acceptTypes[] = {L"application/json", nullptr};
        InternetHandle request(WinHttpOpenRequest(connection, L"GET", kServerListPath, nullptr, WINHTTP_NO_REFERER, acceptTypes, WINHTTP_FLAG_SECURE));
        if (!request.IsValid())
        {
            result.error = WinHttpError("Creating the public server request");
            return result;
        }

        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr))
        {
            result.error = WinHttpError("Downloading the public server list");
            return result;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(
                request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX) ||
            statusCode != 200)
        {
            result.error = "The public server service returned HTTP " + std::to_string(statusCode) + ".";
            return result;
        }

        std::string responseBody;
        while (true)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                result.error = WinHttpError("Reading the public server list");
                return result;
            }

            if (available == 0)
                break;

            if (responseBody.size() + available > kMaxServerListBytes)
            {
                result.error = "The public server response was unexpectedly large.";
                return result;
            }

            const size_t previousSize = responseBody.size();
            responseBody.resize(previousSize + available);

            DWORD bytesRead = 0;
            if (!WinHttpReadData(request, responseBody.data() + previousSize, available, &bytesRead))
            {
                result.error = WinHttpError("Reading the public server response");
                return result;
            }

            responseBody.resize(previousSize + bytesRead);
            if (bytesRead == 0)
                break;
        }

        const nlohmann::json document = nlohmann::json::parse(responseBody);
        if (!document.contains("servers") || !document["servers"].is_array())
        {
            result.error = "The public server service returned an invalid response.";
            return result;
        }

        for (const auto& entry : document["servers"])
        {
            if (!entry.is_object())
                continue;

            PublicServer server;
            server.name = entry.value("name", "Unnamed server");
            server.description = entry.value("desc", "");
            server.address = entry.value("ip", "");
            server.version = entry.value("version", "unknown");

            const int port = entry.value("port", 10578);
            const int playerCount = entry.value("player_count", 0);
            const int maxPlayerCount = entry.value("max_player_count", 0);
            server.port = static_cast<uint16_t>(std::clamp(port, 1, 65535));
            server.playerCount = static_cast<uint16_t>(std::clamp(playerCount, 0, 65535));
            server.maxPlayerCount = static_cast<uint16_t>(std::clamp(maxPlayerCount, 0, 65535));
            server.passwordProtected = entry.value("pass", false);

            if (server.name.size() > 100)
                server.name.resize(100);
            if (server.description.size() > 512)
                server.description.resize(512);
            if (server.address.size() > 255)
                server.address.resize(255);
            if (server.version.size() > 64)
                server.version.resize(64);

            if (!server.address.empty())
                result.servers.emplace_back(std::move(server));
        }
    }
    catch (const std::exception& exception)
    {
        result.error = std::string("Could not parse the public server list: ") + exception.what();
    }
    catch (...)
    {
        result.error = "Could not load the public server list.";
    }

    return result;
}

void ImGuiOverlayService::RefreshPublicServers() noexcept
{
    if (m_serverListLoading)
        return;

    m_serverListLoading = true;
    m_serverListError.clear();

    try
    {
        m_serverListFuture = std::async(std::launch::async, &ImGuiOverlayService::FetchPublicServers);
    }
    catch (const std::exception& exception)
    {
        m_serverListLoading = false;
        m_serverListError = std::string("Could not start the server list request: ") + exception.what();
        spdlog::warn("[overlay] {}", m_serverListError);
    }
}

void ImGuiOverlayService::PollPublicServers() noexcept
{
    if (!m_serverListLoading || !m_serverListFuture.valid())
        return;

    if (m_serverListFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    ServerListResult result;
    try
    {
        result = m_serverListFuture.get();
    }
    catch (const std::exception& exception)
    {
        result.error = std::string("The server list request failed: ") + exception.what();
    }

    m_serverListLoading = false;
    m_serverListLoaded = result.error.empty();
    m_serverListError = std::move(result.error);

    if (m_serverListLoaded)
    {
        m_publicServers = std::move(result.servers);

        const bool selectedServerStillExists =
            std::any_of(m_publicServers.begin(), m_publicServers.end(), [this](const PublicServer& acServer) { return MakeServerKey(acServer) == m_selectedServerKey; });
        if (!selectedServerStillExists)
        {
            m_selectedServerKey.clear();
            m_serverPasswordBuffer[0] = '\0';
        }

        spdlog::info("[overlay] loaded {} public servers", m_publicServers.size());
    }
    else
    {
        spdlog::warn("[overlay] public server list failed: {}", m_serverListError);
    }
}

void ImGuiOverlayService::ToggleFavorite(const PublicServer& acServer) noexcept
{
    const std::string key = MakeServerKey(acServer);
    if (m_favoriteServers.contains(key))
        m_favoriteServers.erase(key);
    else
        m_favoriteServers.emplace(key);

    SaveSettings();
}

void ImGuiOverlayService::LoadSettings() noexcept
{
    try
    {
        std::ifstream file(GetNativeOverlaySettingsPath());
        if (!file)
            return;

        const nlohmann::json settings = nlohmann::json::parse(file);
        if (settings.contains("favorites") && settings["favorites"].is_array())
        {
            for (const auto& favorite : settings["favorites"])
            {
                if (favorite.is_string())
                    m_favoriteServers.emplace(favorite.get<std::string>());
            }
        }

        m_hideFullServers = settings.value("hide_full_servers", true);
        m_hidePasswordServers = settings.value("hide_password_servers", false);
        m_hideVersionMismatch = settings.value("hide_version_mismatch", false);
        m_partyHudEnabled = settings.value("show_party_hud", true);
        m_partyHudAutoHide = settings.value("auto_hide_party_hud", false);
        m_networkHudEnabled = settings.value("show_network_hud", false);
        m_uiScale = std::clamp(settings.value("ui_scale", 1.f), 0.80f, 1.50f);
        m_partyHudAnchor = std::clamp(settings.value("party_hud_anchor", 0), 0, 3);
        m_partyHudOffsetX = std::clamp(settings.value("party_hud_offset_x", 2.f), 0.f, 84.f);
        m_partyHudOffsetY = std::clamp(settings.value("party_hud_offset_y", 8.f), 2.f, 50.f);

        m_partyHudAutoHideSeconds = settings.value("party_hud_auto_hide_seconds", 1);
        if (m_partyHudAutoHideSeconds != 1 && m_partyHudAutoHideSeconds != 3 && m_partyHudAutoHideSeconds != 5)
            m_partyHudAutoHideSeconds = 1;
    }
    catch (const std::exception& exception)
    {
        spdlog::warn("[overlay] could not load native UI settings: {}", exception.what());
    }
}

void ImGuiOverlayService::SaveSettings() const noexcept
{
    try
    {
        const std::filesystem::path settingsPath = GetNativeOverlaySettingsPath();
        std::error_code error;
        std::filesystem::create_directories(settingsPath.parent_path(), error);
        if (error)
        {
            spdlog::warn("[overlay] could not create native UI settings directory '{}': {}", settingsPath.parent_path().string(), error.message());
            return;
        }

        nlohmann::json settings;
        settings["favorites"] = m_favoriteServers;
        settings["hide_full_servers"] = m_hideFullServers;
        settings["hide_password_servers"] = m_hidePasswordServers;
        settings["hide_version_mismatch"] = m_hideVersionMismatch;
        settings["show_party_hud"] = m_partyHudEnabled;
        settings["auto_hide_party_hud"] = m_partyHudAutoHide;
        settings["party_hud_auto_hide_seconds"] = m_partyHudAutoHideSeconds;
        settings["party_hud_anchor"] = m_partyHudAnchor;
        settings["party_hud_offset_x"] = m_partyHudOffsetX;
        settings["party_hud_offset_y"] = m_partyHudOffsetY;
        settings["show_network_hud"] = m_networkHudEnabled;
        settings["ui_scale"] = m_uiScale;

        std::ofstream file(settingsPath, std::ios::trunc);
        if (!file)
        {
            spdlog::warn("[overlay] could not open native UI settings file '{}'", settingsPath.string());
            return;
        }

        file << settings.dump(2);
        if (!file)
            spdlog::warn("[overlay] could not write native UI settings file '{}'", settingsPath.string());
    }
    catch (const std::exception& exception)
    {
        spdlog::warn("[overlay] could not save native UI settings: {}", exception.what());
    }
}

std::string ImGuiOverlayService::MakeServerKey(const PublicServer& acServer)
{
    return MakeEndpoint(acServer.address, acServer.port);
}

std::string ImGuiOverlayService::MakeEndpoint(const std::string& acAddress, uint16_t aPort)
{
    if (acAddress.find(':') != std::string::npos && !(acAddress.starts_with('[') && acAddress.ends_with(']')))
        return "[" + acAddress + "]:" + std::to_string(aPort);

    return acAddress + ":" + std::to_string(aPort);
}

bool ImGuiOverlayService::IsVersionCompatible(const PublicServer& acServer) noexcept
{
    const std::string clientVersion = NormalizeVersion(PROTOCOL_VERSION);
    const std::string serverVersion = NormalizeVersion(acServer.version);

    return clientVersion == serverVersion;
}

std::string ImGuiOverlayService::DescribeConnectionError(const std::string& acDetail, bool& aIsWarning) noexcept
{
    aIsWarning = false;

    const nlohmann::json detail = nlohmann::json::parse(acDetail, nullptr, false);
    if (detail.is_discarded() || !detail.is_object())
        return acDetail.empty() ? "The connection failed for an unknown reason." : acDetail;

    const std::string error = detail.value("error", "no_reason");

    if (error == "non_default_install")
    {
        aIsWarning = true;
        return "A non-vanilla installation was detected. Extra mods or Creation Club content can cause synchronization problems.";
    }
    if (error == "bad_uGridsToLoad")
    {
        aIsWarning = true;
        return "uGridsToLoad must be set to 5 for multiplayer.";
    }
    if (error == "set_time_public_server")
    {
        aIsWarning = true;
        return "Changing the server time is disabled on public servers.";
    }
    if (error == "wrong_password")
        return "The server password is incorrect.";
    if (error == "server_full")
        return "The server is full.";
    if (error == "wrong_version")
    {
        const auto data = detail.value("data", nlohmann::json::object());
        return "Version mismatch. Client: " + data.value("version", std::string("unknown")) + ", server expects: " + data.value("expectedVersion", std::string("unknown")) + ".";
    }
    if (error == "mods_mismatch")
        return "Your enabled mod list does not match the server.";
    if (error == "client_mods_disallowed")
        return "The server does not allow the active client-side mod configuration.";
    if (error == "network_timeout")
        return "The server did not answer before the connection timed out. It may be offline or blocking UDP traffic.";
    if (error == "local_network_error")
        return "The local network stack could not maintain the connection.";
    if (error == "cannot_resolve_address")
        return "The server address could not be resolved.";
    if (error == "server_closed_during_authentication")
        return "The server closed the connection during authentication without returning a reason.";
    if (error == "no_reason")
        return "The server rejected the connection without providing a reason.";

    return "Connection failed: " + error + ".";
}
