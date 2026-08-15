zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(
    oled_raw_ssd1306_128x64.c
    oled_gfx.c
    oled_font_5x7.c
    oled_ui_left.c
)
