#pragma once

// Overlay nativo em ImGui — substituto do overlay CEF para Wine/Proton, onde o
// CefInitialize crasha (ver docs/cef-proton.md e OverlayService::Create). Reusa a
// infra do ImguiService (sinal OnDraw + render sobre D3D11, que funciona sob
// Proton) e chama as mesmas ações de transporte/serviços que a UI web chamava.
//
// Ativado sob Wine; no Windows o overlay CEF continua sendo usado.

#include <cstdint>
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

struct ConnectedEvent;
struct DisconnectedEvent;
struct ConnectionErrorEvent;
struct NotifyChatMessageBroadcast;
struct NotifyPlayerJoined;
struct NotifyPlayerLeft;
struct NotifyPlayerLevel;

class ImGuiOverlayService
{
public:
    ImGuiOverlayService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher, ImguiService& aImguiService);
    ~ImGuiOverlayService() noexcept;

    TP_NOCOPYMOVE(ImGuiOverlayService);

    // Alterna a visibilidade do overlay (dirigido pelo InputService, ex.: F2).
    void Toggle() noexcept;
    void SetVisible(bool aVisible) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

private:
    void OnDraw() noexcept;

    void DrawMainWindow() noexcept;
    void DrawConnectionTab() noexcept;
    void DrawServerBrowserTab() noexcept;
    void DrawPlayersTab() noexcept;
    void DrawChatWindow() noexcept;

    void OnConnected(const ConnectedEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnConnectionError(const ConnectionErrorEvent& acEvent) noexcept;
    void OnChatMessage(const NotifyChatMessageBroadcast& acMessage) noexcept;
    void OnPlayerJoined(const NotifyPlayerJoined& acMessage) noexcept;
    void OnPlayerLeft(const NotifyPlayerLeft& acMessage) noexcept;
    void OnPlayerLevel(const NotifyPlayerLevel& acMessage) noexcept;

    void AddSystemMessage(const std::string& acText) noexcept;
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
    void LoadFavorites() noexcept;
    void SaveFavorites() const noexcept;

    [[nodiscard]] static std::string MakeServerKey(const PublicServer& acServer);
    [[nodiscard]] static std::string MakeEndpoint(const std::string& acAddress, uint16_t aPort);
    [[nodiscard]] static bool IsVersionCompatible(const PublicServer& acServer) noexcept;
    [[nodiscard]] static std::string DescribeConnectionError(const std::string& acDetail, bool& aIsWarning) noexcept;

    struct RemotePlayer
    {
        std::string name;
        uint16_t level = 0;
    };

    struct ChatLine
    {
        std::string author; // vazio = mensagem de sistema
        std::string text;
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
    std::vector<PublicServer> m_publicServers;
    std::unordered_set<std::string> m_favoriteServers;
    std::future<ServerListResult> m_serverListFuture;

    entt::scoped_connection m_drawConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_connectionErrorConnection;
    entt::scoped_connection m_chatConnection;
    entt::scoped_connection m_playerJoinedConnection;
    entt::scoped_connection m_playerLeftConnection;
    entt::scoped_connection m_playerLevelConnection;
};
