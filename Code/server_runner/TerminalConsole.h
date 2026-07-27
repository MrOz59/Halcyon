#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/sink.h>
#include <uv.h>

class TerminalConsole
{
public:
    using CommandHandler = std::function<void(const std::string&)>;
    using InterruptHandler = std::function<void()>;

    TerminalConsole() = default;
    ~TerminalConsole();

    TerminalConsole(const TerminalConsole&) = delete;
    TerminalConsole(TerminalConsole&&) = delete;
    TerminalConsole& operator=(const TerminalConsole&) = delete;
    TerminalConsole& operator=(TerminalConsole&&) = delete;

    bool Initialize(uv_loop_t& aLoop, CommandHandler aCommandHandler, InterruptHandler aInterruptHandler);
    void Shutdown();

    void WriteLog(std::string_view acMessage);
    void SetPrompt(std::string aPrompt);

    [[nodiscard]] bool IsInteractive() const noexcept;

private:
    enum class EscapeState
    {
        None,
        Escape,
        ControlSequence
    };

    static void AllocateBuffer(uv_handle_t* apHandle, size_t aSuggestedSize, uv_buf_t* apBuffer);
    static void ReadInput(uv_stream_t* apStream, ssize_t aRead, const uv_buf_t* acpBuffer);

    void HandleInput(std::string_view acInput);
    void HandleByte(unsigned char aByte);
    void SubmitLine();
    void MoveHistory(int aDirection);

    void ClearCurrentLineLocked();
    void RedrawPromptLocked();
    void WriteRawLocked(std::string_view acText);
    void EnsureTrailingNewlineLocked(std::string_view acText);

private:
    mutable std::mutex m_mutex;

    uv_loop_t* m_pLoop{};
    uv_tty_t m_tty{};
    bool m_ttyInitialized{};
    bool m_reading{};
    bool m_interactive{};
    bool m_stopping{};

    CommandHandler m_commandHandler;
    InterruptHandler m_interruptHandler;

    std::string m_prompt{"> "};
    std::string m_input;
    std::vector<std::string> m_history;
    size_t m_historyIndex{};
    EscapeState m_escapeState{EscapeState::None};
};

std::shared_ptr<spdlog::sinks::sink> MakeTerminalConsoleSink();
TerminalConsole& GetTerminalConsole();
