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

#include <DInputHook.hpp>

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
constexpr size_t kMaxChatLines = 200;
constexpr size_t kMaxServerListBytes = 4 * 1024 * 1024;
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
    m_playerJoinedConnection = aDispatcher.sink<NotifyPlayerJoined>().connect<&ImGuiOverlayService::OnPlayerJoined>(this);
    m_playerLeftConnection = aDispatcher.sink<NotifyPlayerLeft>().connect<&ImGuiOverlayService::OnPlayerLeft>(this);
    m_playerLevelConnection = aDispatcher.sink<NotifyPlayerLevel>().connect<&ImGuiOverlayService::OnPlayerLevel>(this);

    m_statusLine = "Not connected";
    LoadFavorites();
}

ImGuiOverlayService::~ImGuiOverlayService() noexcept = default;

void ImGuiOverlayService::Toggle() noexcept
{
    SetVisible(!m_visible);
}

void ImGuiOverlayService::SetVisible(bool aVisible) noexcept
{
    // DInputHook intercepta a tecla de toggle antes do WndProc. Reconciliar o
    // estado mesmo quando a visibilidade não mudou evita deixar teclado/mouse
    // presos caso o toggle seja recebido fora do jogo ou durante um load.
    TiltedPhoques::DInputHook::Get().SetEnabled(aVisible);

    if (aVisible)
    {
        while (ShowCursor(TRUE) < 0)
            ;
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
            ;
    }

    if (m_visible == aVisible)
        return;

    m_visible = aVisible;
    spdlog::info("[overlay] native ImGui UI {}", m_visible ? "opened" : "closed");

    if (m_visible && !m_serverListLoaded && !m_serverListLoading)
        RefreshPublicServers();
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
    m_connecting = false;
    m_statusLine = "Connected";
    m_errorLine.clear();
    AddSystemMessage("Connected to server.");
}

void ImGuiOverlayService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_connecting = false;
    m_statusLine = "Disconnected";
    m_players.clear();

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
    PollPublicServers();

    if (!m_visible)
        return;

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

    DrawSkyrimWindowFrame();
    DrawSkyrimSectionHeading("SKYRIM TOGETHER");

    const ImVec4 statusColor = m_connected ? ImVec4(0.62f, 0.72f, 0.50f, 1.f) : (m_connecting ? SkyrimAccent() : ImVec4(0.58f, 0.57f, 0.53f, 1.f));
    ImGui::TextColored(statusColor, "%s", m_statusLine.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  |  Protocol %s  |  Build %s  |  F2 or Esc to close", PROTOCOL_VERSION, BUILD_COMMIT);

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
    else if (ImGui::Button(m_connecting ? "CANCEL" : "DISCONNECT", ImVec2(140.f, 0.f)))
    {
        World& world = m_world;
        world.GetRunner().Queue([&world] { world.GetTransport().Close(); });
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
        SaveFavorites();

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
    ImGui::Text("%zu players online", m_players.size());
    ImGui::Separator();

    if (m_players.empty())
    {
        ImGui::TextDisabled(m_connected ? "No other players are currently visible." : "Connect to a server to see its players.");
        return;
    }

    if (ImGui::BeginTable("players_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("PLAYER");
        ImGui::TableSetupColumn("LEVEL", ImGuiTableColumnFlags_WidthFixed, 90.f);
        ImGui::TableHeadersRow();

        for (const auto& [id, player] : m_players)
        {
            TP_UNUSED(id);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(player.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", player.level);
        }

        ImGui::EndTable();
    }
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

    DrawSkyrimWindowFrame();
    DrawSkyrimSectionHeading("GLOBAL CHAT");

    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("chat_scroll", ImVec2(0, -footer), true))
    {
        if (m_chat.empty())
            ImGui::TextDisabled("Chat messages and system notifications will appear here.");

        for (const auto& line : m_chat)
        {
            if (line.author.empty())
                ImGui::TextColored(SkyrimAccent(0.72f), "%s", line.text.c_str());
            else
            {
                ImGui::TextColored(SkyrimAccent(), "%s", line.author.c_str());
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

    ImGui::BeginDisabled(!m_connected);
    ImGui::SetNextItemWidth(-90.f);
    const bool submitted = ImGui::InputTextWithHint(
        "##chat_input", m_connected ? "Type a global message" : "Connect to a server to chat", m_chatInputBuffer, std::size(m_chatInputBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const bool sendClicked = ImGui::Button("SEND");
    ImGui::EndDisabled();

    if ((submitted || sendClicked) && m_chatInputBuffer[0] != '\0')
    {
        SendChatMessageRequest request;
        request.MessageType = ChatMessageType::kGlobalChat;
        request.ChatMessage = m_chatInputBuffer;
        m_transport.Send(request);
        m_chatInputBuffer[0] = '\0';
    }

    ImGui::End();
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

    SaveFavorites();
}

void ImGuiOverlayService::LoadFavorites() noexcept
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
    }
    catch (const std::exception& exception)
    {
        spdlog::warn("[overlay] could not load native UI settings: {}", exception.what());
    }
}

void ImGuiOverlayService::SaveFavorites() const noexcept
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
