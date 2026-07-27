#include "TerminalConsole.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
class TerminalConsoleSink final : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& acMessage) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(acMessage, formatted);
        GetTerminalConsole().WriteLog(std::string_view(formatted.data(), formatted.size()));
    }

    void flush_() override
    {
        std::fflush(stdout);
    }
};
}

TerminalConsole::~TerminalConsole()
{
    Shutdown();
}

TerminalConsole& GetTerminalConsole()
{
    static TerminalConsole s_console;
    return s_console;
}

std::shared_ptr<spdlog::sinks::sink> MakeTerminalConsoleSink()
{
    return std::make_shared<TerminalConsoleSink>();
}

bool TerminalConsole::Initialize(uv_loop_t& aLoop, CommandHandler aCommandHandler, InterruptHandler aInterruptHandler)
{
    std::scoped_lock lock(m_mutex);

    if (m_ttyInitialized)
        return m_interactive;

    m_pLoop = &aLoop;
    m_commandHandler = std::move(aCommandHandler);
    m_interruptHandler = std::move(aInterruptHandler);
    m_stopping = false;

    if (uv_guess_handle(0) != UV_TTY || uv_guess_handle(1) != UV_TTY)
    {
        m_interactive = false;
        return false;
    }

#ifdef _WIN32
    const HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (outputHandle != INVALID_HANDLE_VALUE)
    {
        DWORD outputMode{};
        if (GetConsoleMode(outputHandle, &outputMode))
            SetConsoleMode(outputHandle, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    const int initResult = uv_tty_init(m_pLoop, &m_tty, 0, 1);
    if (initResult < 0)
    {
        m_interactive = false;
        return false;
    }

    m_ttyInitialized = true;
    m_tty.data = this;

    const int modeResult = uv_tty_set_mode(&m_tty, UV_TTY_MODE_RAW);
    if (modeResult < 0)
    {
        uv_close(reinterpret_cast<uv_handle_t*>(&m_tty), nullptr);
        uv_run(m_pLoop, UV_RUN_NOWAIT);
        m_ttyInitialized = false;
        m_interactive = false;
        return false;
    }

    const int readResult = uv_read_start(reinterpret_cast<uv_stream_t*>(&m_tty), AllocateBuffer, ReadInput);
    if (readResult < 0)
    {
        uv_tty_reset_mode();
        uv_close(reinterpret_cast<uv_handle_t*>(&m_tty), nullptr);
        uv_run(m_pLoop, UV_RUN_NOWAIT);
        m_ttyInitialized = false;
        m_interactive = false;
        return false;
    }

    m_reading = true;
    m_interactive = true;
    m_historyIndex = m_history.size();

    RedrawPromptLocked();
    return true;
}

void TerminalConsole::Shutdown()
{
    std::scoped_lock lock(m_mutex);

    if (m_stopping)
        return;

    m_stopping = true;

    if (m_reading)
    {
        uv_read_stop(reinterpret_cast<uv_stream_t*>(&m_tty));
        m_reading = false;
    }

    if (m_ttyInitialized)
    {
        uv_tty_reset_mode();

        if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&m_tty)))
            uv_close(reinterpret_cast<uv_handle_t*>(&m_tty), nullptr);

        if (m_pLoop)
            uv_run(m_pLoop, UV_RUN_NOWAIT);

        m_ttyInitialized = false;
    }

    if (m_interactive)
    {
        ClearCurrentLineLocked();
        WriteRawLocked("\n");
    }

    m_interactive = false;
}

void TerminalConsole::SetPrompt(std::string aPrompt)
{
    std::scoped_lock lock(m_mutex);

    m_prompt = std::move(aPrompt);

    if (m_interactive)
        RedrawPromptLocked();
}

bool TerminalConsole::IsInteractive() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_interactive;
}

void TerminalConsole::WriteLog(std::string_view acMessage)
{
    std::scoped_lock lock(m_mutex);

    if (!m_interactive)
    {
        if (!acMessage.empty())
            std::fwrite(acMessage.data(), 1, acMessage.size(), stdout);

        EnsureTrailingNewlineLocked(acMessage);
        std::fflush(stdout);
        return;
    }

    ClearCurrentLineLocked();

    if (!acMessage.empty())
        WriteRawLocked(acMessage);

    EnsureTrailingNewlineLocked(acMessage);
    RedrawPromptLocked();
}

void TerminalConsole::AllocateBuffer(uv_handle_t*, size_t aSuggestedSize, uv_buf_t* apBuffer)
{
    const size_t size = std::max<size_t>(aSuggestedSize, 64);
    auto* pBuffer = static_cast<char*>(std::malloc(size));

    if (!pBuffer)
    {
        *apBuffer = uv_buf_init(nullptr, 0);
        return;
    }

    *apBuffer = uv_buf_init(pBuffer, static_cast<unsigned int>(size));
}

void TerminalConsole::ReadInput(uv_stream_t* apStream, ssize_t aRead, const uv_buf_t* acpBuffer)
{
    auto* pConsole = static_cast<TerminalConsole*>(apStream->data);

    if (pConsole && aRead > 0 && acpBuffer && acpBuffer->base)
        pConsole->HandleInput(std::string_view(acpBuffer->base, static_cast<size_t>(aRead)));

    if (acpBuffer && acpBuffer->base)
        std::free(acpBuffer->base);

    if (!pConsole || aRead >= 0)
        return;

    if (aRead == UV_EOF)
        pConsole->Shutdown();
}

void TerminalConsole::HandleInput(std::string_view acInput)
{
    for (const unsigned char byte : acInput)
        HandleByte(byte);
}

void TerminalConsole::HandleByte(unsigned char aByte)
{
    CommandHandler commandHandler;
    InterruptHandler interruptHandler;

    {
        std::unique_lock lock(m_mutex);

        if (!m_interactive || m_stopping)
            return;

        if (m_escapeState == EscapeState::Escape)
        {
            if (aByte == '[')
            {
                m_escapeState = EscapeState::ControlSequence;
                return;
            }

            m_escapeState = EscapeState::None;
        }
        else if (m_escapeState == EscapeState::ControlSequence)
        {
            m_escapeState = EscapeState::None;

            if (aByte == 'A')
            {
                lock.unlock();
                MoveHistory(-1);
                return;
            }

            if (aByte == 'B')
            {
                lock.unlock();
                MoveHistory(1);
                return;
            }

            return;
        }

        if (aByte == 0x1B)
        {
            m_escapeState = EscapeState::Escape;
            return;
        }

        if (aByte == 0x03)
        {
            interruptHandler = m_interruptHandler;
            ClearCurrentLineLocked();
            WriteRawLocked("^C\n");
            m_input.clear();
            m_historyIndex = m_history.size();
        }
        else if (aByte == '\r' || aByte == '\n')
        {
            lock.unlock();
            SubmitLine();
            return;
        }
        else if (aByte == 0x7F || aByte == 0x08)
        {
            if (!m_input.empty())
            {
                m_input.pop_back();
                RedrawPromptLocked();
            }
            return;
        }
        else if (aByte == 0x04)
        {
            if (m_input.empty())
            {
                interruptHandler = m_interruptHandler;
                ClearCurrentLineLocked();
                WriteRawLocked("^D\n");
            }
        }
        else if (aByte >= 0x20)
        {
            m_input.push_back(static_cast<char>(aByte));
            RedrawPromptLocked();
            return;
        }
        else
        {
            return;
        }
    }

    if (interruptHandler)
        interruptHandler();
}

void TerminalConsole::SubmitLine()
{
    CommandHandler commandHandler;
    std::string command;

    {
        std::scoped_lock lock(m_mutex);

        if (!m_interactive || m_stopping)
            return;

        ClearCurrentLineLocked();
        WriteRawLocked(m_prompt);
        WriteRawLocked(m_input);
        WriteRawLocked("\n");

        command = m_input;
        m_input.clear();

        if (!command.empty())
        {
            if (m_history.empty() || m_history.back() != command)
                m_history.push_back(command);

            constexpr size_t kMaximumHistorySize = 100;
            if (m_history.size() > kMaximumHistorySize)
                m_history.erase(m_history.begin());
        }

        m_historyIndex = m_history.size();
        commandHandler = m_commandHandler;
    }

    if (!command.empty() && commandHandler)
        commandHandler(command);

    std::scoped_lock lock(m_mutex);
    if (m_interactive && !m_stopping)
        RedrawPromptLocked();
}

void TerminalConsole::MoveHistory(int aDirection)
{
    std::scoped_lock lock(m_mutex);

    if (!m_interactive || m_history.empty())
        return;

    if (aDirection < 0)
    {
        if (m_historyIndex > 0)
            --m_historyIndex;
    }
    else
    {
        if (m_historyIndex < m_history.size())
            ++m_historyIndex;
    }

    if (m_historyIndex < m_history.size())
        m_input = m_history[m_historyIndex];
    else
        m_input.clear();

    RedrawPromptLocked();
}

void TerminalConsole::ClearCurrentLineLocked()
{
    WriteRawLocked("\r\x1b[2K");
}

void TerminalConsole::RedrawPromptLocked()
{
    ClearCurrentLineLocked();
    WriteRawLocked(m_prompt);
    WriteRawLocked(m_input);
    std::fflush(stdout);
}

void TerminalConsole::WriteRawLocked(std::string_view acText)
{
    if (!acText.empty())
        std::fwrite(acText.data(), 1, acText.size(), stdout);
}

void TerminalConsole::EnsureTrailingNewlineLocked(std::string_view acText)
{
    if (acText.empty() || acText.back() != '\n')
        WriteRawLocked("\n");
}
