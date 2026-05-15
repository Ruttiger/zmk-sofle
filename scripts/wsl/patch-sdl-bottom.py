#!/usr/bin/env python3
"""patch-sdl-bottom.py — Patch display_sdl_bottom.c for WSL native_posix compatibility.

Called by preview-display-wsl.sh before west build.

Applies three changes to the Zephyr SDL display bottom-half driver:

1. SDL_RENDERER_ACCELERATED → SDL_RENDERER_SOFTWARE
   Avoids requiring a GPU-capable renderer in headless/WSL environments.

2. Re-initialise SDL video from the Zephyr kernel thread.
   In native_posix_64, SDL_Init() is called via NATIVE_TASK which runs on a
   different POSIX thread from the Zephyr kernel thread that later calls
   SDL_CreateWindow / SDL_RenderPresent.  WSL/XWayland crashes when the X11
   Display* is used across POSIX threads (even with XInitThreads).  Quitting
   and re-initialising the video subsystem from the Zephyr thread ensures that
   all subsequent SDL/X11 calls share the same connection.

3. Block SIGUSR1/SIGUSR2 during SDL/X11 calls in sdl_display_write_bottom.
   Zephyr native_posix uses SIGUSR1 for cooperative thread context switching.
   If SIGUSR1 is delivered in the middle of XPutImage (which is not
   async-signal-safe), it corrupts X11's internal state and causes a SIGSEGV.
   Masking the signal around SDL rendering calls prevents the race condition.
"""
import re
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <path/to/display_sdl_bottom.c>")
    sys.exit(1)

path = sys.argv[1]

with open(path, "r") as f:
    content = f.read()

# ── 1. Software renderer ──────────────────────────────────────────────────────
content = content.replace("SDL_RENDERER_ACCELERATED", "SDL_RENDERER_SOFTWARE")

# ── 2. SDL re-init from the Zephyr thread ────────────────────────────────────
reinit_code = (
    "\n"
    "\t/* Permanently block SIGUSR1/SIGUSR2 in the display thread.\n"
    "\t * Zephyr native_posix uses these signals for thread context switching.\n"
    "\t * X11/SDL functions are not async-signal-safe; if a signal fires\n"
    "\t * mid-XCreateWindow, XRenderPresent etc., it corrupts X11 state\n"
    "\t * and causes SIGSEGV.  The display thread yields via k_sleep so\n"
    "\t * blocking here is safe — other threads still get CPU time. */\n"
    "\t{\n"
    "\t\tsigset_t _zmk_disp_sigset;\n"
    "\t\tsigemptyset(&_zmk_disp_sigset);\n"
    "\t\tsigaddset(&_zmk_disp_sigset, SIGUSR1);\n"
    "\t\tsigaddset(&_zmk_disp_sigset, SIGUSR2);\n"
    "\t\tpthread_sigmask(SIG_BLOCK, &_zmk_disp_sigset, NULL);\n"
    "\t}\n"
    "\n"
    "\t/* WSL/native_posix cross-thread fix: re-initialise SDL video\n"
    "\t * from this thread.  SDL_Init() runs via NATIVE_TASK on a\n"
    "\t * different POSIX thread; SDL_RenderPresent() runs on the\n"
    "\t * Zephyr kernel thread.  XWayland crashes when the X11 Display*\n"
    "\t * is used across threads.  Re-init here makes all SDL/X11 calls\n"
    "\t * share one thread-local connection. */\n"
    "\tif (SDL_WasInit(SDL_INIT_VIDEO)) {\n"
    "\t\tSDL_QuitSubSystem(SDL_INIT_VIDEO);\n"
    "\t}\n"
    "\tif (SDL_Init(SDL_INIT_VIDEO) < 0) {\n"
    '\t\tnsi_print_warning("SDL_Init failed: %s", SDL_GetError());\n'
    "\t\treturn -1;\n"
    "\t}\n"
)

content, n = re.subn(
    r"(\n\t\*window = SDL_CreateWindow\b)",
    reinit_code + r"\1",
    content,
    count=1,
)

if n == 0:
    print("WARNING: Could not find SDL_CreateWindow insertion point — patch skipped.")
else:
    print(f"Patched {path} (SDL_RENDERER_SOFTWARE + thread re-init + signal masking)")

# ── 3. Block SIGUSR1/SIGUSR2 around sdl_display_write_bottom ─────────────────
# Zephyr native_posix uses SIGUSR1 for cooperative thread switching.
# If SIGUSR1 fires inside XPutImage (not async-signal-safe), it corrupts
# X11 state → SIGSEGV.  Mask the signal for the duration of each frame.

signal_block = (
    "\tsigset_t _zmk_sigset, _zmk_old_sigset;\n"
    "\tsigemptyset(&_zmk_sigset);\n"
    "\tsigaddset(&_zmk_sigset, SIGUSR1);\n"
    "\tsigaddset(&_zmk_sigset, SIGUSR2);\n"
    "\tpthread_sigmask(SIG_BLOCK, &_zmk_sigset, &_zmk_old_sigset);\n"
)
signal_unblock = (
    "\tpthread_sigmask(SIG_SETMASK, &_zmk_old_sigset, NULL);\n"
)

# Add signal.h include after existing includes
if "#include <signal.h>" not in content:
    content = content.replace(
        "#include <SDL.h>",
        "#include <SDL.h>\n#include <signal.h>\n#include <pthread.h>"
    )

# Insert signal block right before SDL_UpdateTexture (unique to write_bottom).
content = content.replace(
    "\tSDL_UpdateTexture(texture, &rect, buf, 4 * rect.w);\n",
    signal_block + "\tSDL_UpdateTexture(texture, &rect, buf, 4 * rect.w);\n",
)

# Insert signal unblock before the closing } of sdl_display_write_bottom.
# SDL_RenderPresent(renderer) (with pointer renderer, not *renderer) is
# unique to write_bottom; sdl_display_init_bottom uses SDL_RenderPresent(*renderer).
content = re.sub(
    r"(\t\tSDL_RenderPresent\(renderer\);\n\t\})\n\}",
    r"\1\n" + signal_unblock + "}",
    content,
    count=1,
)

with open(path, "w") as f:
    f.write(content)
