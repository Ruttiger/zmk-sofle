# Only compile the custom status screen when ZMK_DISPLAY_STATUS_SCREEN_CUSTOM
# is selected. With BUILT_IN, ZMK provides zmk_display_status_screen() itself.
if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library_sources(custom_status_screen.c)

  # --- Panel source selection ---
  # Each panel lives in panels/<name>/ and provides eyelash_panel_screen().
  # Switch panels by setting CONFIG_EYELASH_PANEL_xxx in config/eyelash_sofle.conf.
  if(CONFIG_EYELASH_PANEL_PLACEHOLDER)
    zephyr_library_sources(panels/placeholder/panel.c)
    zephyr_library_sources(assets/placeholder.c)
  elseif(CONFIG_EYELASH_PANEL_LAYER_BATTERY)
    zephyr_library_sources(panels/layer_battery/panel.c)
    # No static assets required for this panel
  endif()
endif()
