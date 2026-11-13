# Abobo's Big Adventure -> Miyoo Mini Plus: status

## What this is
Abobo's Big Adventure is a Flash (ActionScript 2) game, so it cannot be "recompiled". This project is a small native
Flash runtime written in C (AVM1 script interpreter + timeline/display-list engine + software renderer) that runs the
game's ORIGINAL scripts and art, with an SDL2 front end for Onion OS.

## Verified on x86 (headless runner + SDL front end under SDL's dummy video driver)
- Original SWF extracted from the exe; 5766 bitmaps, 641 sounds, ~2830 sprites, ~6500 shapes, 28 buttons, ~880 KB of AS2 converted.
- AS2 interpreter runs the game's code. The real flow works end to end with the original scripts:
  preloader -> "CLICK ANYWHERE TO PLAY" (click) -> Newgrounds card -> intro (A = skip) -> real main menu
  (START GAME / LEVEL SELECT / ACHIEVEMENTS) -> START GAME -> arcade intro -> controls screen -> level 1 with background, HUD, Abobo.
- Level-select screen renders and responds to keys. Vector solids (polygon fill), masks, buttons implemented.
- Fixed this session: gotoAndStop/Play state handling (jump ops must not change play state) and Flash's timeline depth offset
  (-16384) so script-attached clips no longer overwrite timeline-placed ones (this was deleting the level background).
- src/platform_sdl.c builds in two variants (SDL2, or SDL 1.2 via -DUSE_SDL1). The SDL2 variant was run against the real libSDL2
  with dummy video/audio drivers (150 frames, frame verified). The SDL 1.2 variant was only syntax-checked against a stub header.

## Open problems
1. NEVER built for ARM / never run on a Miyoo (no ARM toolchain or network in the build sandbox). First run on the device may need fixes.
2. Audio is only a thin SDL_mixer layer, untested with real files (sounds are WAV converted by package_onion.sh).
3. Speed on the Cortex-A7 is unmeasured. Sandbox numbers: ~1.7 ms/frame rendering average on x86 (1:1 blit fast path added); an A7 is
   roughly 10-15x slower, so 24 fps is likely but not guaranteed in heavy scenes (masks, big fades).
4. Gameplay beyond the first minutes of level 1 has only been exercised with scripted inputs; later levels, bosses, 2P mode untested.
5. Dynamic text fields (DefineEditText/fonts) are not rendered; BitmapData/filters/blend modes/ColorTransform-on-MC are stubs;
   Newgrounds API, medals popup and the ad slideshow overlay are hidden/stubbed; SharedObject saves do not persist.
6. Masks use shape bounds when a mask shape has no bitmap/solid polygon.

## Controls (planned mapping, in platform_sdl.c)
D-pad = arrows, A = key A, B = key S, X = Space, Y = D, Start = P (pause) + click, Select = M (mute),
L1 = Enter, hold R1 = pointer mode (D-pad moves pointer, A clicks), Menu = quit.

## Build
Desktop headless test:   make headless && ABOBO_INPUT="210:c:128:127;212:cu:128:127" ./abobo_headless assets 900 100
  (writes /tmp/frame_NNNN.ppm; env ABOBO_DEBUG=1, ABOBO_TREE=N for a clip-tree dump)
Miyoo:  enter the union-miyoomini-toolchain shell (github.com/shauninman/union-miyoomini-toolchain), mount this folder, `make miyoo`,
        then `./package_onion.sh` and copy AbobosBigAdventure/ to /mnt/SDCARD/App/.

## Regenerating data
tools/extract1.py, extract2.py (assets), convert.py (scene.bin), pack_bitmaps.py (bitmaps.pak). They read /tmp/abobo.swf,
which is the SWF cut out of the end of abobosbigadventure_PC.exe (assets/abobo_extracted.swf).
