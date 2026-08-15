zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(
    oled_raw_ssd1306_128x32.c
    oled_gfx_portrait.c
    oled_smoke_screen.c
)
