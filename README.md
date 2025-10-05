# Cool Retro Terminal Clone

This project is a fully featured SDL2-based Linux terminal emulator with CRT-style rendering. It provides a nostalgic aesthetic inspired by vintage monitors while remaining usable for everyday shell work.

## Features

- SDL2 renderer with animated CRT effects including scanlines, vignette, and screen noise
- Truecolor terminal emulation with ANSI/VT escape sequence support, including SGR color modes and cursor control
- PTY-backed shell session that respects the current user's login shell and window resizing
- Dynamic font atlas built on top of SDL_ttf with bold, italic, and underline rendering
- Configurable monospace font via the `CRT_FONT_PATH` environment variable
- Default window size of 1920×1080 with resize handling and DPI awareness

## Building

### Prerequisites

- A C++20 compiler (GCC or Clang recommended)
- `pkg-config`
- SDL2 development libraries
- SDL2_ttf development libraries

On Debian/Ubuntu you can install dependencies via:

```bash
sudo apt-get install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev
```

### Build

```bash
make
```

The resulting executable will be located at `build/cool_retro_terminal`.

## Running

From the project root:

```bash
make run
```

The emulator will spawn your login shell inside a pseudo terminal. Keyboard input (including control sequences and arrow keys) is forwarded to the shell. Text is rendered using the first available monospace font from a set of common candidates. To force a specific font, set `CRT_FONT_PATH` to the path of a TTF font file before launching the application.

## License

This project is provided without an explicit license. Contact the authors for licensing information if you plan to redistribute or modify the software.

