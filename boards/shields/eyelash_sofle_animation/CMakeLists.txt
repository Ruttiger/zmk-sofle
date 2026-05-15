# Only compile the custom status screen when ZMK_DISPLAY_STATUS_SCREEN_CUSTOM
# is selected. With BUILT_IN, ZMK provides zmk_display_status_screen() itself.
if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library_sources(custom_status_screen.c)

  # --- Animation asset ---
  # Replace placeholder.c with your generated animation after running gif-convert.py:
  #   python3 scripts/gif-convert.py input.gif --no-register -W 160 -H 68
  # Then swap the line below and update the #include in custom_status_screen.c.
  zephyr_library_sources(assets/placeholder.c)
endif()
