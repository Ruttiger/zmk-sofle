if(CONFIG_NICE_OLED_ON AND NOT CONFIG_SHIELD_OLED_RESOLUTION_DIAG)
  zephyr_library()
  zephyr_library_sources(zmk-nice-oled-128x64.c)

  # Upstream nice!oled calls rotate_canvas() from screen.c and
  # screen_peripheral.c. Wrap only that geometry helper so those two files
  # remain unchanged.
  zephyr_link_libraries(-Wl,--wrap=rotate_canvas)
endif()
