CC ?= cc

flecs: c-src/flecs.c c-src/helper.c c-src/flecs.h Makefile
	$(CC) -shared -o flecs -O2 -fPIC ./c-src/flecs.c ./c-src/helper.c -DFLECS_SCRIPT_MATH -Wl,-undefined,dynamic_lookup