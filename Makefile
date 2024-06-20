default:
	gcc main.c -o program.exe -O1 -Wall -Wno-missing-braces -I raylib/ -L raylib/ -lraylib -lopengl32 -lgdi32 -lwinmm