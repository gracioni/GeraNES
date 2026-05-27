# Video

## Where Video Options Live

Open `Options > Video`.

This menu includes presentation settings rather than emulation correctness settings. You can change the look without changing the ROM itself.

## VSync

`Options > Video > VSync`

Use this if you need to balance latency, tearing, and display smoothness.

## Filter

`Options > Video > Filter`

Common choices include:

- nearest-neighbor style output for crisp pixels
- bilinear filtering for a softer scaled image

## Shader

`Options > Video > Shader`

GeraNES supports shader-based post-processing. Shaders can simulate scanlines, CRT behavior, smoothing, and other display styles.

Use shaders when you want a more stylized image. Disable them if you prefer a cleaner or lower-overhead presentation.

GeraNES does not limit you to a single shader pass. Shaders can be stacked, which means you can combine multiple effects in sequence instead of choosing only one.

Shader parameters are also configurable. When a shader exposes adjustable values, you can tune those parameters to control the final look more precisely.

This is useful when you want to:

- combine effects such as scaling and CRT-style filtering
- fine-tune scanline intensity, bloom, curvature, sharpness, or similar shader-specific settings
- build a custom visual preset instead of relying on a single default effect

GeraNES also lets you save shader presets. This is useful when you want to switch quickly between different shader configurations without rebuilding the stack and parameter values by hand every time.

## Palette

`Options > Video > Palette`

Palettes change how NES colors are interpreted. GeraNES includes multiple palette presets and also supports custom palette management.

This is useful if you prefer:

- a more saturated image
- a softer composite-like look
- a palette closer to another emulator or hardware capture style

## Overscan

`Options > Video > Overscan`

Overscan controls how much of the outer image area is shown. Some games contain edge garbage or transition artifacts that look better when cropped.

## Scale Mode

`Options > Video > Scale Mode`

The available modes are designed for different priorities:

- aspect fit for a safe default
- stretch to fill if you want full screen coverage
- pixel perfect variants if you want cleaner integer-style scaling

## Fullscreen

Use the fullscreen action when you want a dedicated viewing mode:

- Shortcut: `Alt+F`
- Additional behavior is available under `Options > Video > Fullscreen Mode`

## FPS Counter

`Options > Video > Show FPS`

This is useful for troubleshooting performance or presentation issues.
