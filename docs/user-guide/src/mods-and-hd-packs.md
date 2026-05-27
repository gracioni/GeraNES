# Mods and HD Packs

## What The Mod System Is For

GeraNES can load visual mod packs that replace or augment the original NES graphics output. This is mainly intended for HD-style packs built around the `hires.txt` workflow used by Mesen.

In practice, this means GeraNES is compatible with Mesen HD packs to the extent that the pack uses features GeraNES currently supports.

!!! note
    Compatibility is not "all packs guaranteed." Some packs may rely on `hires.txt` rules or behaviors that GeraNES does not implement yet.

## Supported Mod Sources

You can load a mod from:

- `Mod > Load > ZIP File`
- `Mod > Load > Folder`

If a game is already running, selecting a mod causes GeraNES to reload the current ROM so the mod can be applied immediately.

## Basic Requirements

A supported pack is expected to contain a `hires.txt` file. That file describes how the pack should behave.

If the selected mod does not contain `hires.txt`, it will not load as a graphics mod.

## What A Pack Can Influence

Depending on the pack contents and the features it uses, a mod can provide:

- higher-resolution tile replacements
- background replacement layers
- pack-defined overscan
- optional ROM patching through `hires.txt`
- additional audio configuration used by the mod system

## ROM Patching

Some packs include a `<patch>` entry in `hires.txt`.

When that happens, GeraNES can:

- validate the expected base ROM hash
- apply the patch
- cache a patched ROM for use with the mod

If the ROM hash does not match what the pack expects, the mod may be rejected to avoid applying the wrong patch to the wrong game.

## Scale And Presentation

Packs can request a higher internal mod scale. GeraNES uses that scale when composing modded output, which is why HD-style graphics can appear sharper or more detailed than the original 1x NES frame.

Some packs also define their own overscan values, which can change how much of the outer image is visible.

## Useful Mod Menu Actions

After loading a pack, these menu items are especially useful:

- `Mod > Clear`
- `Mod > Show original graphics`

`Show original graphics` lets you temporarily compare the base game image against the modded presentation.

## Debugging And Inspection

If you want to inspect how a mod is being drawn, GeraNES also includes:

- `Screen Pixel Inspector`

When mod output is active, that tool can help you compare original and modded pixels.

## Practical Advice

- Prefer packs made for the same ROM revision you are using.
- If a pack includes patch logic, use the ROM version it expects.
- If graphics look incomplete, the pack may be using unsupported `hires.txt` features.
- If you want to go back to the unmodified game, use `Mod > Clear`.

## Short Version

GeraNES supports Mesen-style HD pack workflows, especially packs built around supported `hires.txt` features. Load them from a ZIP or folder, expect best results when the ROM version matches the pack, and use `Show original graphics` or `Clear` when comparing behavior.
