OS := $(shell uname)

OBJS = LTexture.cpp client.cpp

CC = g++

COMPILER_FLAGS = 

ifeq ($(OS), Darwin) #MacOs
	LINKER_FLAGS = -F /Library/Frameworks -framework SDL2 -I /Library/Frameworks/SDL2.framework/Headers -framework SDL2_ttf -I /Library/Frameworks/SDL2_ttf.framework/Headers
else
	LINKER_FLAGS = -lSDL2 -lSDL2_ttf
endif

OBJ_NAME = client

all:
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)
debug: 
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -g -o $(OBJ_NAME)
