#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include "LTexture.h"
#include "../shared.h"

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 640;
const int MAX_COMMAND_LEN = 128;

struct GlobalState
{
    char gamePhase;
    char playersConnectd;
    char playersReady;
};

bool init();
void close();

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;
TTF_Font *gFont;
LTexture *gTextTexture;
SDL_Color textColor = {0, 0, 0};

bool init()
{
    bool success = true;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        success = false;
    }
    else
    {
        if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"))
        {
            printf("Warning: Linear texture filtering not enabled!");
        }

        gWindow = SDL_CreateWindow("gnibbles", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (gWindow == NULL)
        {
            printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
            gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
            if (gRenderer == NULL)
            {
                printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
                success = false;
            }
            else
            {
                SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

                int imgFlags = IMG_INIT_PNG;
                if (!(IMG_Init(imgFlags) & imgFlags))
                {
                    printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
                    success = false;
                }
            }
        }
    }

    return success;
}

bool loadMedia()
{
    //Loading success flag
    bool success = true;

    //Open the font
    gFont = TTF_OpenFont("font.ttf", 28);
    if (gFont == NULL)
    {
        printf("Failed to load lazy font! SDL_ttf Error: %s\n", TTF_GetError());
        success = false;
    }

    return success;
}

void close()
{
    //Destroy window
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;
    gRenderer = NULL;

    //Quit SDL subsystems
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char *args[])
{
    if (!init() || !loadMedia())
    {
        printf("Failed to initialize!\n");
    }
    else
    {
        gTextTexture->init(gRenderer, gFont);
        struct sockaddr_in servaddr;
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);

        char *sep = strchr(args[1], ':');
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(atoi(sep + 1));
        sep[0] = 0;
        inet_aton(args[1], &servaddr.sin_addr);

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
            printf("Can't connect to server\n");
            return 1;
        }

        char commandBuff[MAX_COMMAND_LEN];
        read(sockfd, commandBuff, MAX_COMMAND_LEN);

        printf("%d\n", commandBuff[0]);

        //Main loop flag
        bool quit = false;

        //Event handler
        SDL_Event e;

        bool ready = false;

        GlobalState *gameState = new GlobalState();
        gameState->gamePhase = WAITING_FOR_PLAYERS;

        //While application is running
        while (!quit)
        {

            //Handle events on queue
            while (SDL_PollEvent(&e) != 0)
            {
                //User requests quit
                if (e.type == SDL_QUIT)
                {
                    quit = true;
                }
                else if (e.type == SDL_KEYDOWN)
                {
                    printf("%d\n", e.key.keysym.sym);
                    //Select surfaces based on key press
                    switch (e.key.keysym.sym)
                    {
                    case SDLK_q:
                        quit = true;
                        break;
                    case SDLK_SPACE:
                        ready = !ready;
                    default:
                        break;
                    }
                }
            }

            SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderClear(gRenderer);

            if (gameState->gamePhase == WAITING_FOR_PLAYERS)
            {
                int n = write(sockfd, &ready, sizeof(ready));
                n = read(sockfd, gameState, sizeof(gameState));
                printf("Curr state:\n%d players\n%d ready\n%d gamePhase\n", gameState->playersConnectd, gameState->playersReady, gameState->gamePhase);
                SDL_Rect fillRect = {SCREEN_WIDTH / 4, SCREEN_HEIGHT / 4, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2};
                if (ready)
                {
                    SDL_SetRenderDrawColor(gRenderer, 0x00, 0xFF, 0x00, 0xFF);
                }
                else
                {
                    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0x00, 0x00, 0xFF);
                }

                SDL_RenderFillRect(gRenderer, &fillRect);
            }
            else
            {
                printf("WTF\n");
            }

            //Update screen
            SDL_RenderPresent(gRenderer);
        }
    }

    //Free resources and close SDL
    close();

    return 0;
}
