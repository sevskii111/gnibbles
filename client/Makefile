OS := $(shell uname)

OBJS = client.cpp

CC = g++

COMPILER_FLAGS = 

ifeq ($(OS), Darwin) #MacOs
	LINKER_FLAGS = -I include -L lib -l SDL2-2.0.0 -l SDL2_image-2.0.0
else
	LINKER_FLAGS = -lSDL2 -lSDL2_image
endif

OBJ_NAME = client

all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)