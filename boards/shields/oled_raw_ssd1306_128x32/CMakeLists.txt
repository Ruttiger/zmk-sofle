zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(
    oled_raw_ssd1306_128x32.c
    oled_gfx_portrait.c
    oled_screen_controller.c
)

if(CONFIG_SHIELD_OLED_UI_32X128_V1)
    zephyr_library_sources(
        oled_font_5x7.c
        oled_gfx_text.c
        oled_ui_v1.c
    )
else()
    zephyr_library_sources(oled_smoke_screen.c)
endif()
