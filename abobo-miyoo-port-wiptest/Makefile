# Desktop headless test build:   make headless
# Miyoo Mini (Onion) cross build, inside the union-miyoomini-toolchain shell:   make miyoo
SRC=src/rt_core.c src/rt_interp.c src/rt_mc.c src/rt_render.c src/rt_builtins.c
headless:
	gcc -O2 -w -o abobo_headless src/main_headless.c $(SRC) -lz -lm
miyoo1:
	$(CROSS_COMPILE)gcc -O2 -w -DUSE_SDL1 -o abobo src/platform_sdl.c $(SRC) -lSDL -lSDL_mixer -lz -lm
miyoo:
	$(CROSS_COMPILE)gcc -O2 -w -o abobo src/platform_sdl.c $(SRC) $$(sdl2-config --cflags --libs) -lSDL2_mixer -lz -lm
desktop:
	gcc -O2 -w -o abobo_sdl src/platform_sdl.c $(SRC) $$(sdl2-config --cflags --libs) -lSDL2_mixer -lz -lm
