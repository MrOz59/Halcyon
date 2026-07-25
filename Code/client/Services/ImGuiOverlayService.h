#pragma once

// Native ImGui overlay used instead of CEF on Wine/Proton, where CefInitialize
// crashes (see docs/cef-proton.md and OverlayService::Create). It reuses the
// existing D3D11 ImGui renderer and invokes the same transport messages and
// client services as the web UI.
//
// Enabled under Wine only; native Windows keeps using the CEF overlay.

#include <cstdint>
#include <chrono>
#include <deque>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <TiltedCore/Stl.hpp>

#include <entt/entt.hpp>

struct World;
struct TransportService;
struct ImguiService;
struct ImGuiInputTextCallbackData;

struct ConnectedEvent;
struct DisconnectedEvent;
struct ConnectionErrorEvent;
struct PartyJoinedEvent;
struct PartyLeftEvent;
struct NotifyChatMessageBroadcast;
struct NotifyPartyInfo;
struct NotifyPartyInvite;
struct NotifyPlayerCellChanged;
struct NotifyPlayerDialogue;
struct NotifyPlayerHealthUpdate;
struct NotifyPlayerJoined;
struct NotifyPlayerList;
struct NotifyPlayerLeft;
struct NotifyPlayerLevel;

class ImGuiOverlayService
{
public:
    ImGuiOverlayService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher, ImguiService& aImguiService);
    ~ImGuiOverlayService() noexcept;

    TP_NOCOPYMOVE(ImGuiOverlayService);

    // Toggles the interactive overlay (driven by InputService, e.g. F2).
    void Toggle() noexcept;
    void SetVisible(bool aVisible) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

    // Makes local client/service notifications visible without going through CEF.
    void PushSystemMessage(const std::string& acText) noexcept;

private:
    void OnDraw() noexcept;

    void DrawMainWindow() noexcept;
    void DrawConnectionTab() noexcept;
    void DrawServerBrowserTab() noexcept;
    void DrawPlayersTab() noexcept;
    void DrawPartyTab() noexcept;
    void DrawSettingsTab() noexcept;
    void DrawChatWindow() noexcept;
    void DrawPartyHud() noexcept;
    void DrawPartyInviteNotice() noexcept;
    void DrawNetworkHud() noexcept;
    void DrawNotifications() noexcept;

    void OnConnected(const ConnectedEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnConnectionError(const ConnectionErrorEvent& acEvent) noexcept;
    void OnChatMessage(const NotifyChatMessageBroadcast& acMessage) noexcept;
    void OnPlayerDialogue(const NotifyPlayerDialogue& acMessage) noexcept;
    void OnPlayerList(const NotifyPlayerList& acMessage) noexcept;
    void OnPlayerJoined(const NotifyPlayerJoined& acMessage) noexcept;
    void OnPlayerLeft(const NotifyPlayerLeft& acMessage) noexcept;
    void OnPlayerLevel(const NotifyPlayerLevel& acMessage) noexcept;
    void OnPlayerCellChanged(const NotifyPlayerCellChanged& acMessage) noexcept;
    void OnPlayerHealthUpdate(const NotifyPlayerHealthUpdate& acMessage) noexcept;
    void OnPartyInfo(const NotifyPartyInfo& acMessage) noexcept;
    void OnPartyInvite(const NotifyPartyInvite& acMessage) noexcept;
    void OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept;
    void OnPartyLeft(const PartyLeftEvent& acEvent) noexcept;
    void OnWaitingFor3DRemoved(entt::registry& aRegistry, entt::entity aEntity) noexcept;
    void OnPlayerComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) noexcept;

    void AddSystemMessage(const std::string& acText) noexcept;
    void AddChatMessage(uint8_t aType, const std::string& acAuthor, const std::string& acText) noexcept;
    void FlashPartyHud() noexcept;
    void SubmitChatInput() noexcept;
    void SendChatMessage(uint8_t aType, const std::string& acText) noexcept;
    static int ChatHistoryCallback(ImGuiInputTextCallbackData* apData) noexcept;
    void Connect(const std::string& acAddress, uint16_t aPort, const std::string& acPassword) noexcept;

    struct PublicServer
    {
        std::string name;
        std::string description;
        std::string address;
        std::string version;
        uint16_t port = 10578;
        uint16_t playerCount = 0;
        uint16_t maxPlayerCount = 0;
        bool passwordProtected = false;
    };

    struct ServerListResult
    {
        std::vector<PublicServer> servers;
        std::string error;
    };

    static ServerListResult FetchPublicServers() noexcept;
    void RefreshPublicServers() noexcept;
    void PollPublicServers() noexcept;
    void ToggleFavorite(const PublicServer& acServer) noexcept;
    void LoadSettings() noexcept;
    void SaveSettings() const noexcept;

    [[nodiscard]] static std::string MakeServerKey(const PublicServer& acServer);
    [[nodiscard]] static std::string MakeEndpoint(const std::string& acAddress, uint16_t aPort);
    [[nodiscard]] static bool IsVersionCompatible(const PublicServer& acServer) noexcept;
    [[nodiscard]] static std::string DescribeConnectionError(const std::string& acDetail, bool& aIsWarning) noexcept;

    struct RemotePlayer
    {
        std::string name;
        uint16_t level = 0;
        std::string cellName;
        float health = -1.f;
        bool loaded = false;
    };

    struct ChatLine
    {
        uint8_t type = 0;
        std::string author; // Empty for system messages.
        std::string text;
        std::chrono::system_clock::time_point timestamp;
    };

    struct Notification
    {
        uint8_t type = 0;
        std::string text;
        std::chrono::steady_clock::time_point expiresAt;
    };

    World& m_world;
    TransportService& m_transport;

    bool m_visible = false;
    bool m_connected = false;
    bool m_connecting = false;
    bool m_scrollChatToBottom = false;
    bool m_serverListLoading = false;
    bool m_serverListLoaded = false;
    bool m_hideFullServers = true;
    bool m_hidePasswordServers = false;
    bool m_hideVersionMismatch = false;
    bool m_partyHudEnabled = true;
    bool m_partyHudAutoHide = false;
    bool m_networkHudEnabled = false;
    float m_uiScale = 1.f;
    float m_partyHudOffsetX = 2.f;
    float m_partyHudOffsetY = 8.f;
    int m_partyHudAutoHideSeconds = 1;
    int m_partyHudAnchor = 0;
    int m_chatChannel = 1;

    char m_addressBuffer[128]{"127.0.0.1"};
    int m_port = 10578;
    char m_passwordBuffer[128]{};
    char m_chatInputBuffer[512]{};
    char m_serverSearchBuffer[128]{};
    char m_serverPasswordBuffer[128]{};
    std::string m_statusLine;
    std::string m_warningLine;
    std::string m_errorLine;
    std::string m_selectedServerKey;
    std::string m_serverListError;

    std::unordered_map<uint32_t, RemotePlayer> m_players;
    std::deque<ChatLine> m_chat;
    std::deque<Notification> m_notifications;
    std::vector<std::string> m_chatHistory;
    int m_chatHistoryIndex = -1;
    std::vector<PublicServer> m_publicServers;
    std::unordered_set<std::string> m_favoriteServers;
    std::unordered_map<uint32_t, uint64_t> m_invitedPlayers;
    std::unordered_set<uint32_t> m_acceptingInvites;
    std::chrono::steady_clock::time_point m_partyHudVisibleUntil;
    std::chrono::steady_clock::time_point m_revealAvailableAt;
    std::future<ServerListResult> m_serverListFuture;

    entt::scoped_connection m_drawConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_connectionErrorConnection;
    entt::scoped_connection m_chatConnection;
    entt::scoped_connection m_playerDialogueConnection;
    entt::scoped_connection m_playerListConnection;
    entt::scoped_connection m_playerJoinedConnection;
    entt::scoped_connection m_playerLeftConnection;
    entt::scoped_connection m_playerLevelConnection;
    entt::scoped_connection m_playerCellConnection;
    entt::scoped_connection m_playerHealthConnection;
    entt::scoped_connection m_partyInfoConnection;
    entt::scoped_connection m_partyInviteConnection;
    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_partyLeftConnection;
    entt::scoped_connection m_playerAddedConnection;
    entt::scoped_connection m_playerRemovedConnection;
};
