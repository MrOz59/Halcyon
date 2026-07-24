#include <TiltedOnlinePCH.h>

#include <Services/ImGuiOverlayService.h>
#include <Services/ImguiService.h>
#include <Services/TransportService.h>

#include <World.h>

#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/ConnectionErrorEvent.h>

#include <Messages/NotifyChatMessageBroadcast.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/NotifyPlayerLeft.h>
#include <Messages/NotifyPlayerLevel.h>
#include <Messages/SendChatMessageRequest.h>

#include <ChatMessageTypes.h>

#include <imgui.h>

namespace
{
constexpr size_t kMaxChatLines = 200;
}

ImGuiOverlayService::ImGuiOverlayService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher, ImguiService& aImguiService)
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_drawConnection = aImguiService.OnDraw.connect<&ImGuiOverlayService::OnDraw>(this);
    m_connectedConnection = aDispatcher.sink<ConnectedEvent>().connect<&ImGuiOverlayService::OnConnected>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&ImGuiOverlayService::OnDisconnected>(this);
    m_connectionErrorConnection = aDispatcher.sink<ConnectionErrorEvent>().connect<&ImGuiOverlayService::OnConnectionError>(this);
    m_chatConnection = aDispatcher.sink<NotifyChatMessageBroadcast>().connect<&ImGuiOverlayService::OnChatMessage>(this);
    m_playerJoinedConnection = aDispatcher.sink<NotifyPlayerJoined>().connect<&ImGuiOverlayService::OnPlayerJoined>(this);
    m_playerLeftConnection = aDispatcher.sink<NotifyPlayerLeft>().connect<&ImGuiOverlayService::OnPlayerLeft>(this);
    m_playerLevelConnection = aDispatcher.sink<NotifyPlayerLevel>().connect<&ImGuiOverlayService::OnPlayerLevel>(this);

    m_statusLine = "Not connected";
}

ImGuiOverlayService::~ImGuiOverlayService() noexcept = default;

void ImGuiOverlayService::Toggle() noexcept
{
    m_visible = !m_visible;
}

void ImGuiOverlayService::AddSystemMessage(const std::string& acText) noexcept
{
    m_chat.push_back(ChatLine{{}, acText});
    while (m_chat.size() > kMaxChatLines)
        m_chat.pop_front();
    m_scrollChatToBottom = true;
}

void ImGuiOverlayService::OnConnected(const ConnectedEvent&) noexcept
{
    m_connected = true;
    m_statusLine = "Connected";
    AddSystemMessage("Connected to server.");
}

void ImGuiOverlayService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_connected = false;
    m_statusLine = "Disconnected";
    m_players.clear();
    AddSystemMessage("Disconnected from server.");
}

void ImGuiOverlayService::OnConnectionError(const ConnectionErrorEvent& acEvent) noexcept
{
    m_connected = false;
    m_statusLine = std::string("Connection error: ") + acEvent.ErrorDetail.c_str();
    AddSystemMessage(m_statusLine);
}

void ImGuiOverlayService::OnChatMessage(const NotifyChatMessageBroadcast& acMessage) noexcept
{
    m_chat.push_back(ChatLine{acMessage.PlayerName.c_str(), acMessage.ChatMessage.c_str()});
    while (m_chat.size() > kMaxChatLines)
        m_chat.pop_front();
    m_scrollChatToBottom = true;
}

void ImGuiOverlayService::OnPlayerJoined(const NotifyPlayerJoined& acMessage) noexcept
{
    m_players[acMessage.PlayerId] = RemotePlayer{acMessage.Username.c_str(), acMessage.Level};
    AddSystemMessage(std::string(acMessage.Username.c_str()) + " joined.");
}

void ImGuiOverlayService::OnPlayerLeft(const NotifyPlayerLeft& acMessage) noexcept
{
    m_players.erase(acMessage.PlayerId);
    AddSystemMessage(std::string(acMessage.Username.c_str()) + " left.");
}

void ImGuiOverlayService::OnPlayerLevel(const NotifyPlayerLevel& acMessage) noexcept
{
    auto it = m_players.find(acMessage.PlayerId);
    if (it != m_players.end())
        it->second.level = acMessage.NewLevel;
}

void ImGuiOverlayService::OnDraw() noexcept
{
    if (!m_visible)
        return;

    DrawConnectionWindow();
    DrawPlayersWindow();
    DrawChatWindow();
}

void ImGuiOverlayService::DrawConnectionWindow() noexcept
{
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Skyrim Together"))
    {
        ImGui::TextUnformatted(m_statusLine.c_str());
        ImGui::Separator();

        ImGui::BeginDisabled(m_connected);
        ImGui::InputText("Address", m_addressBuffer, std::size(m_addressBuffer));
        ImGui::InputInt("Port", &m_port);
        ImGui::InputText("Password", m_passwordBuffer, std::size(m_passwordBuffer), ImGuiInputTextFlags_Password);
        ImGui::EndDisabled();

        if (!m_connected)
        {
            if (ImGui::Button("Connect"))
            {
                std::string address = m_addressBuffer;
                if (address == "localhost")
                    address = "127.0.0.1";

                const uint16_t port = m_port > 0 ? static_cast<uint16_t>(m_port) : 10578;
                const std::string endpoint = address + ":" + std::to_string(port);

                m_transport.SetServerPassword(m_passwordBuffer);
                m_statusLine = "Connecting to " + endpoint + "...";

                World& world = m_world;
                world.GetRunner().Queue([&world, endpoint] { world.GetTransport().Connect(endpoint); });
            }
        }
        else
        {
            if (ImGui::Button("Disconnect"))
            {
                World& world = m_world;
                world.GetRunner().Queue([&world] { world.GetTransport().Close(); });
            }
        }
    }
    ImGui::End();
}

void ImGuiOverlayService::DrawPlayersWindow() noexcept
{
    ImGui::SetNextWindowSize(ImVec2(220, 260), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(380, 40), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Players"))
    {
        ImGui::Text("%d online", static_cast<int>(m_players.size()));
        ImGui::Separator();

        for (const auto& [id, player] : m_players)
            ImGui::Text("%s (lvl %u)", player.name.c_str(), player.level);
    }
    ImGui::End();
}

void ImGuiOverlayService::DrawChatWindow() noexcept
{
    ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Chat"))
    {
        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("chat_scroll", ImVec2(0, -footer), true))
        {
            for (const auto& line : m_chat)
            {
                if (line.author.empty())
                    ImGui::TextDisabled("%s", line.text.c_str());
                else
                    ImGui::Text("%s: %s", line.author.c_str(), line.text.c_str());
            }

            if (m_scrollChatToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
                m_scrollChatToBottom = false;
            }
        }
        ImGui::EndChild();

        ImGui::BeginDisabled(!m_connected);
        const bool submitted = ImGui::InputText("##chat_input", m_chatInputBuffer, std::size(m_chatInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool sendClicked = ImGui::Button("Send");
        ImGui::EndDisabled();

        if ((submitted || sendClicked) && m_chatInputBuffer[0] != '\0')
        {
            SendChatMessageRequest request;
            request.MessageType = ChatMessageType::kGlobalChat;
            request.ChatMessage = m_chatInputBuffer;
            m_transport.Send(request);

            m_chatInputBuffer[0] = '\0';
        }
    }
    ImGui::End();
}
