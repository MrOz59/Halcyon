#pragma once

// Overlay nativo em ImGui — substituto do overlay CEF para Wine/Proton, onde o
// CefInitialize crasha (ver docs/cef-proton.md e OverlayService::Create). Reusa a
// infra do ImguiService (sinal OnDraw + render sobre D3D11, que funciona sob
// Proton) e chama as mesmas ações de transporte/serviços que a UI web chamava.
//
// Ativado sob Wine; no Windows o overlay CEF continua sendo usado.

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

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

    void DrawConnectionWindow() noexcept;
    void DrawPlayersWindow() noexcept;
    void DrawChatWindow() noexcept;

    void OnConnected(const ConnectedEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnConnectionError(const ConnectionErrorEvent& acEvent) noexcept;
    void OnChatMessage(const NotifyChatMessageBroadcast& acMessage) noexcept;
    void OnPlayerJoined(const NotifyPlayerJoined& acMessage) noexcept;
    void OnPlayerLeft(const NotifyPlayerLeft& acMessage) noexcept;
    void OnPlayerLevel(const NotifyPlayerLevel& acMessage) noexcept;

    void AddSystemMessage(const std::string& acText) noexcept;

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
    bool m_scrollChatToBottom = false;

    char m_addressBuffer[128]{"127.0.0.1"};
    int m_port = 10578;
    char m_passwordBuffer[128]{};
    char m_chatInputBuffer[512]{};
    std::string m_statusLine;

    std::unordered_map<uint32_t, RemotePlayer> m_players;
    std::deque<ChatLine> m_chat;

    entt::scoped_connection m_drawConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_connectionErrorConnection;
    entt::scoped_connection m_chatConnection;
    entt::scoped_connection m_playerJoinedConnection;
    entt::scoped_connection m_playerLeftConnection;
    entt::scoped_connection m_playerLevelConnection;
};
