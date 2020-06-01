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
#include "colors.h"
#include "../shared.h"

const int SCREEN_WIDTH = 640;
const int MENU_HEIGHT = 100;
const int GAME_HEIGHT = 640;
const int SCREEN_HEIGHT = GAME_HEIGHT + MENU_HEIGHT;
const int MAX_COMMAND_LEN = 128;

bool init();
void close();

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;
TTF_Font *gBigFont, *gMidFont, *gSmallFont;
LTexture *gTextTexture;

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
  gSmallFont = TTF_OpenFont("font.ttf", 24);
  gMidFont = TTF_OpenFont("font.ttf", 36);
  gBigFont = TTF_OpenFont("font.ttf", 60);
  if (gSmallFont == NULL || gMidFont == NULL || gBigFont == NULL)
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
  if (!init() || TTF_Init() || !loadMedia())
  {
    printf("Failed to initialize!\n");
  }
  else
  {
    gTextTexture = new LTexture();
    gTextTexture->init(gRenderer);
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

    char ind;
    read(sockfd, &ind, sizeof(ind));

    //Main loop flag
    bool quit = false;

    //Event handler
    SDL_Event e;

    bool ready = false;

    char gamePhase = WAITING_FOR_PLAYERS;

    char mapSize = -1;
    char **map;
    //While application is running
    while (!quit)
    {
      char arrowPressed = 0;
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
            break;
          case SDLK_UP:
            arrowPressed = 1;
            break;
          case SDLK_RIGHT:
            arrowPressed = 2;
            break;
          case SDLK_DOWN:
            arrowPressed = 3;
            break;
          case SDLK_LEFT:
            arrowPressed = 4;
            break;
          default:
            break;
          }
        }
      }

      SDL_SetRenderDrawColor(gRenderer, DARK);
      SDL_RenderClear(gRenderer);

      if (gamePhase == WAITING_FOR_PLAYERS)
      {
        WaitingState waitingState;
        int n = write(sockfd, &ready, sizeof(ready));
        n = read(sockfd, &waitingState, sizeof(waitingState));
        gamePhase = waitingState.gamePhase;

        char textBuff[128];
        sprintf(textBuff, "%d of %d", waitingState.playersReady, waitingState.playersConnected);
        gTextTexture->loadFromRenderedText(textBuff, gBigFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, (SCREEN_HEIGHT - gTextTexture->getHeight() * 2) / 2);

        sprintf(textBuff, "players are ready");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, (SCREEN_HEIGHT) / 2);

        int textOffset = 5;
        int textYPos = (SCREEN_HEIGHT + gTextTexture->getHeight()) / 2 + gTextTexture->getHeight() + textOffset;
        if (ready)
        {
          sprintf(textBuff, "You are ready");
          gTextTexture->loadFromRenderedText(textBuff, gMidFont, TGREEN);
          gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, textYPos);
        }
        else
        {
          sprintf(textBuff, "You are not ready");
          gTextTexture->loadFromRenderedText(textBuff, gMidFont, TRED);
          gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, textYPos);
        }

        sprintf(textBuff, "Press space to change");
        gTextTexture->loadFromRenderedText(textBuff, gSmallFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, textYPos + gTextTexture->getHeight() + textOffset);
      }
      else if (gamePhase == IN_PROGRESS)
      {
        if (mapSize == -1)
        {
          read(sockfd, &mapSize, sizeof(mapSize));
        }
        for (int i = 0; i < mapSize; i++)
        {
          char currRow[mapSize];
          read(sockfd, currRow, sizeof(currRow));
          for (int j = 0; j < mapSize; j++)
          {
            if (currRow[j])
            {
              if (currRow[j] > 0)
              {
                SDL_Rect fillRect = {SCREEN_WIDTH / mapSize * i, MENU_HEIGHT + GAME_HEIGHT / mapSize * j, SCREEN_WIDTH / mapSize, GAME_HEIGHT / mapSize};
                SDL_SetRenderDrawColor(gRenderer, MAP_COLORS[currRow[j] - 1][0], MAP_COLORS[currRow[j] - 1][1], MAP_COLORS[currRow[j] - 1][2], 0xFF);
                SDL_RenderFillRect(gRenderer, &fillRect);
              }
              else
              {
                char textBuff[2];
                sprintf(textBuff, "%d", -currRow[j]);
                gTextTexture->loadFromRenderedText(textBuff, gMidFont, TGREEN);
                gTextTexture->render(SCREEN_WIDTH / mapSize * i + gTextTexture->getWidth() / 2, MENU_HEIGHT + GAME_HEIGHT / mapSize * j);
              }
            }
          }
        }

        InGameState inGameState;
        read(sockfd, &inGameState, sizeof(inGameState));
        gamePhase = inGameState.gamePhase;

        int OFFSET = 10;

        {
          SDL_Rect fillRect = {0, 0, SCREEN_WIDTH, MENU_HEIGHT};
          SDL_SetRenderDrawColor(gRenderer, BLACK);
          SDL_RenderFillRect(gRenderer, &fillRect);
        }

        char textBuff[128];
        sprintf(textBuff, "Color");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        int squereSize = MENU_HEIGHT - gTextTexture->getHeight() - OFFSET * 3;
        gTextTexture->render(OFFSET, OFFSET);

        SDL_Rect fillRect = {OFFSET + (gTextTexture->getWidth() - squereSize) / 2, MENU_HEIGHT - squereSize - OFFSET, squereSize, squereSize};
        SDL_SetRenderDrawColor(gRenderer, MAP_COLORS[ind + 1][0], MAP_COLORS[ind + 1][1], MAP_COLORS[ind + 1][2], 0xFF);
        SDL_RenderFillRect(gRenderer, &fillRect);

        int scoreTextPos = OFFSET * 2 + squereSize / 2 + gTextTexture->getWidth();
        sprintf(textBuff, "Score");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render(scoreTextPos, OFFSET);
        int pTextHeight = gTextTexture->getHeight();
        sprintf(textBuff, "%d", 100);
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render(scoreTextPos, pTextHeight + (MENU_HEIGHT - pTextHeight - gTextTexture->getHeight()) / 2);

        write(sockfd, &arrowPressed, sizeof(arrowPressed));
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
