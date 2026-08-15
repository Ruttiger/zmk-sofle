# Vendor nice!oled baseline

This branch uses `mctechnology17/zmk-nice-oled:main` directly through the
`nice_oled` shield. The OLED hardware remains defined by the
`eyelash_sofle_*` overlays: SSD1306, I2C address `0x3c`, SDA `P0.17` and SCL
`P0.20`.

The upstream widgets render on a portrait canvas and rotate it 90 degrees
clockwise before the display update. The physical SSD1306 viewport is 128x64;
the configured upstream canvas is therefore:

```text
canvas width  = 64
canvas height = 128
rotation      = 90 degrees clockwise
```

The central and peripheral builds use the upstream widget selections produced
by the unmodified `nice_oled` Kconfig defaults. No Ruttiger widget, asset,
animation, font, or position override is added.

The upstream `screen.c` and `screen_peripheral.c` are used unchanged. The
repository supplies only a link-time wrapper for upstream `rotate_canvas()`:
it copies the rectangular 64x128 source, rotates it with the required 32-pixel
offset, and leaves all widget code untouched. This is the only compatibility
code needed for the validated 128x64 panel.

The old `zmk-nice-oled-patches` directory is intentionally not part of the
build and is retained only as historical material.

## Trying upstream animations later

Do not enable these in the stable configuration yet. To test one animation,
change the peripheral settings in `config/eyelash_sofle.conf` as follows and
build again:

```ini
CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL=y
CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL=n
```

Then enable exactly one upstream selector:

```ini
# Cat
CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CAT=y

# Head
CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_HEAD=y

# Pokemon
CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_POKEMON=y

# Spaceman
CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SPACEMAN=y
```

For the upstream Gem/Crystal animation, leave all four selectors above
disabled; upstream `animation.c` then selects its `crystal_imgs` frames.

Return to the stable static configuration by disabling animation and enabling
the VIM static image again.
