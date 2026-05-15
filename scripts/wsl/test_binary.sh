#!/bin/bash
export SDL_VIDEODRIVER=x11
export SDL_RENDER_DRIVER=software
timeout 8 ~/zmk/build/eyelash_sofle_display_preview/zephyr/zmk.exe 2>&1 | cat
echo "EXIT_PIPE:${PIPESTATUS[0]}"
