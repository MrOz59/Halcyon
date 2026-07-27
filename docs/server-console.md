# Halcyon Server Console

**Status:** Prototype

The first Halcyon implementation change improves the dedicated server terminal
without replacing the existing command registry.

## Goals

- prevent log messages from corrupting the command currently being typed;
- handle CRLF and LF input consistently;
- provide command history with Up and Down;
- support Backspace and clean Ctrl+C/Ctrl+D shutdown;
- retain plain output when stdin/stdout are redirected;
- keep rotating file logging independent from the interactive terminal;
- avoid adding a new UI framework dependency.

## Architecture

The console uses the existing libuv event loop and command registry.

A custom spdlog sink temporarily clears the prompt, writes the log record and redraws
the input line. The terminal is placed in raw mode only when both stdin and stdout are
TTY devices.

When running through Docker, systemd, a pipe or redirected output, the sink falls
back to normal line-oriented stdout.

## Controls

| Key | Action |
| --- | --- |
| Enter | Execute command |
| Up / Down | Navigate command history |
| Backspace | Delete previous byte |
| Ctrl+C | Request graceful server shutdown |
| Ctrl+D | Request shutdown when the input line is empty |

## Limitations

- This is a small terminal frontend, not a full administrative dashboard.
- Cursor-left/right editing and autocomplete are not implemented.
- UTF-8 text is preserved as bytes, but Backspace removes one byte rather than one
  complete Unicode code point.
- Terminal resize does not require explicit handling because wrapping remains owned
  by the terminal.
- The implementation requires CI and runtime validation on both Windows and Linux.
