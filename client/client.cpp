#include <SDL2/SDL.h>
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
const int FPS = 24;

bool init();
void close();

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;
TTF_Font *gBigFont, *gMidFont, *gSmallFont, *gFoodFont;
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
      }
    }
  }

  return success;
}

bool loadMedia()
{
  bool success = true;

  gSmallFont = TTF_OpenFont("font.ttf", 24);
  gMidFont = TTF_OpenFont("font.ttf", 36);
  gBigFont = TTF_OpenFont("font.ttf", 60);
  if (gSmallFont == NULL || gMidFont == NULL || gBigFont == NULL)
  {
    printf("Failed to font! SDL_ttf Error: %s\n", TTF_GetError());
    success = false;
  }

  return success;
}

void close()
{
  SDL_DestroyRenderer(gRenderer);
  SDL_DestroyWindow(gWindow);
  gWindow = NULL;
  gRenderer = NULL;
  SDL_Quit();
}

struct GameState
{
  char ind;
  char ready;
  char gamePhase;
  char prevGamePhase;
  char mapSize;
  char **map;
  char scoresLen;
  ScoreboardRecord *scores;
  char direction;
};

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

    GameState gameState;
    gameState.ready = false;
    gameState.gamePhase = WAITING_FOR_PLAYERS;
    gameState.prevGamePhase = -1;
    gameState.mapSize = 0;
    gameState.scoresLen = 0;

    bool quit = false;
    SDL_Event e;
    char textBuff[128];

    if (!read(sockfd, &gameState.ind, sizeof(gameState.ind)))
    {
      quit = true;
    }

    Uint32 startTime = 0;
    Uint32 endTime = 0;
    Uint32 delta = 0;
    short timePerFrame = 1000 / FPS;
    while (!quit)
    {
      if (!startTime)
      {
        startTime = SDL_GetTicks();
      }
      else
      {
        delta = endTime - startTime;
      }
      while (SDL_PollEvent(&e) != 0)
      {
        if (e.type == SDL_QUIT)
        {
          quit = true;
        }
        else if (e.type == SDL_KEYDOWN)
        {
          switch (e.key.keysym.sym)
          {
          case SDLK_q:
            quit = true;
            break;
          case SDLK_SPACE:
            gameState.ready = !gameState.ready;
            break;
          case SDLK_UP:
            gameState.direction = 1;
            break;
          case SDLK_RIGHT:
            gameState.direction = 2;
            break;
          case SDLK_DOWN:
            gameState.direction = 3;
            break;
          case SDLK_LEFT:
            gameState.direction = 4;
            break;
          default:
            break;
          }
        }
      }

      SDL_SetRenderDrawColor(gRenderer, DARK);
      SDL_RenderClear(gRenderer);

      if (gameState.gamePhase == WAITING_FOR_PLAYERS)
      {
        if (gameState.prevGamePhase != WAITING_FOR_PLAYERS)
        {
          gameState.ready = false;
          if (gameState.scoresLen > 0)
          {
            delete[] gameState.scores;
            gameState.scoresLen = 0;
          }
        }

        WaitingState waitingState;
        if (!write(sockfd, &gameState.ready, sizeof(gameState.ready)))
        {
          break;
        }
        if (!read(sockfd, &waitingState, sizeof(waitingState)))
        {
          break;
        }
        gameState.gamePhase = waitingState.gamePhase;

        sprintf(textBuff, "%d of %d", waitingState.playersReady, waitingState.playersConnected);
        gTextTexture->loadFromRenderedText(textBuff, gBigFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, (SCREEN_HEIGHT - gTextTexture->getHeight() * 2) / 2);

        sprintf(textBuff, "players are ready");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, (SCREEN_HEIGHT) / 2);

        int textOffset = 5;
        int textYPos = (SCREEN_HEIGHT + gTextTexture->getHeight()) / 2 + gTextTexture->getHeight() + textOffset;
        if (gameState.ready)
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

        gameState.prevGamePhase = WAITING_FOR_PLAYERS;
      }
      else if (gameState.gamePhase == IN_PROGRESS)
      {
        if (gameState.prevGamePhase != IN_PROGRESS)
        {
          if (!read(sockfd, &gameState.mapSize, sizeof(gameState.mapSize)))
          {
            break;
          }
          gameState.map = new char *[gameState.mapSize];
          for (int i = 0; i < gameState.mapSize; i++)
          {
            gameState.map[i] = new char[gameState.mapSize];
          }
          gFoodFont = TTF_OpenFont("font.ttf", SCREEN_WIDTH / gameState.mapSize);
          gameState.direction = 0;
        }
        char mapStatus;
        if (!read(sockfd, &mapStatus, sizeof(mapStatus)))
        {
          break;
        }
        for (int i = 0; i < gameState.mapSize; i++)
        {
          if (mapStatus)
          {
            char rowStatus;
            if (!read(sockfd, &rowStatus, sizeof(rowStatus)))
            {
              quit = true;
              break;
            }

            if (rowStatus)
            {
              if (!read(sockfd, gameState.map[i], sizeof(*gameState.map[i]) * gameState.mapSize))
              {
                quit = true;
                break;
              }
            }
          }

          char currRow[gameState.mapSize];
          memcpy(currRow, gameState.map[i], sizeof(currRow));

          for (int j = 0; j < gameState.mapSize; j++)
          {
            if (currRow[j])
            {
              if (currRow[j] > 0)
              {
                SDL_Rect fillRect = {SCREEN_WIDTH / gameState.mapSize * i, MENU_HEIGHT + GAME_HEIGHT / gameState.mapSize * j, SCREEN_WIDTH / gameState.mapSize, GAME_HEIGHT / gameState.mapSize};
                SDL_SetRenderDrawColor(gRenderer, MAP_COLORS[currRow[j] - 1][0], MAP_COLORS[currRow[j] - 1][1], MAP_COLORS[currRow[j] - 1][2], 0xFF);
                SDL_RenderFillRect(gRenderer, &fillRect);
              }
              else
              {
                sprintf(textBuff, "%d", -currRow[j]);
                gTextTexture->loadFromRenderedText(textBuff, gFoodFont, TGREEN);
                gTextTexture->render(SCREEN_WIDTH / gameState.mapSize * i + gTextTexture->getWidth() / 2, MENU_HEIGHT + GAME_HEIGHT / gameState.mapSize * j);
              }
            }
          }
        }
        if (quit)
        {
          break;
        }

        InGameState inGameState;
        if (!read(sockfd, &inGameState, sizeof(inGameState)))
        {
          break;
        }
        gameState.gamePhase = inGameState.gamePhase;

        int OFFSET_H = 10;
        int OFFSET_W = SCREEN_WIDTH / gameState.mapSize;

        {
          SDL_Rect fillRect = {0, 0, SCREEN_WIDTH, MENU_HEIGHT};
          SDL_SetRenderDrawColor(gRenderer, BLACK);
          SDL_RenderFillRect(gRenderer, &fillRect);
        }

        sprintf(textBuff, "Color");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        int squereSize = MENU_HEIGHT - gTextTexture->getHeight() - OFFSET_H * 3;
        gTextTexture->render(OFFSET_W, OFFSET_H);
        {
          SDL_Rect fillRect = {OFFSET_W + (gTextTexture->getWidth() - squereSize) / 2, MENU_HEIGHT - squereSize - OFFSET_H, squereSize, squereSize};
          SDL_SetRenderDrawColor(gRenderer, MAP_COLORS[gameState.ind + 1][0], MAP_COLORS[gameState.ind + 1][1], MAP_COLORS[gameState.ind + 1][2], 0xFF);
          SDL_RenderFillRect(gRenderer, &fillRect);
        }

        int scoreTextPos = (SCREEN_WIDTH - gTextTexture->getWidth()) / 2;
        sprintf(textBuff, "SCORE");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render(scoreTextPos, OFFSET_H);

        int pTextHeight = gTextTexture->getHeight();
        int pTextWidth = gTextTexture->getWidth();
        sprintf(textBuff, "%d", inGameState.score);
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render(scoreTextPos + (pTextWidth - gTextTexture->getWidth()) / 2, pTextHeight + (MENU_HEIGHT - pTextHeight - gTextTexture->getHeight()) / 2);

        sprintf(textBuff, "TURN");
        gTextTexture->loadFromRenderedText(textBuff, gMidFont, TWHITE);
        gTextTexture->render(SCREEN_WIDTH - OFFSET_W - gTextTexture->getWidth(), OFFSET_H);
        {
          SDL_Rect fillRect = {SCREEN_WIDTH - (OFFSET_W + (gTextTexture->getWidth() + squereSize) / 2), MENU_HEIGHT - squereSize - OFFSET_H, squereSize, squereSize};
          SDL_SetRenderDrawColor(gRenderer, MAP_COLORS[inGameState.activeWorm + 1][0], MAP_COLORS[inGameState.activeWorm + 1][1], MAP_COLORS[inGameState.activeWorm + 1][2], 0xFF);
          SDL_RenderFillRect(gRenderer, &fillRect);
        }
        if (!write(sockfd, &gameState.direction, sizeof(gameState.direction)))
        {
          break;
        }
        gameState.direction = 0;

        gameState.prevGamePhase = IN_PROGRESS;
      }
      else if (gameState.gamePhase == SCOREBOARD)
      {
        ScoreboardState scoreboardState;
        if (!read(sockfd, &scoreboardState, sizeof(ScoreboardState)))
        {
          break;
        }
        gameState.gamePhase = scoreboardState.gamePhase;
        if (gameState.prevGamePhase == IN_PROGRESS)
        {
          for (int i = 0; i < gameState.mapSize; i++)
          {
            delete[] gameState.map[i];
          }
          delete[] gameState.map;
          gameState.mapSize = 0;

          gameState.scoresLen = scoreboardState.players;
          gameState.scores = new ScoreboardRecord[gameState.scoresLen];
          for (int i = 0; i < gameState.scoresLen; i++)
          {
            if (!read(sockfd, &gameState.scores[i], sizeof(gameState.scores[i])))
            {
              quit = true;
              break;
            }
          }
          if (quit)
          {
            break;
          }
        }

        int TOP_OFFSET = 90;
        int OFFSET = 10;

        sprintf(textBuff, "SCOREBOARD");
        gTextTexture->loadFromRenderedText(textBuff, gBigFont, TWHITE);
        gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, TOP_OFFSET);
        int recordsStart = TOP_OFFSET + gTextTexture->getHeight() + OFFSET;
        for (int i = 0; i < gameState.scoresLen; i++)
        {
          ScoreboardRecord record = gameState.scores[i];
          sprintf(textBuff, "%d", record.score);
          SDL_Color color = {MAP_COLORS[record.ind + 1][0], MAP_COLORS[record.ind + 1][1], MAP_COLORS[record.ind + 1][2]};
          gTextTexture->loadFromRenderedText(textBuff, gBigFont, color);
          gTextTexture->render((SCREEN_WIDTH - gTextTexture->getWidth()) / 2, recordsStart + i * gTextTexture->getHeight());
        }
        char sync = 1;
        if (!write(sockfd, &sync, sizeof(sync)))
        {
          break;
        }

        gameState.prevGamePhase = SCOREBOARD;
      }
      else
      {
        break;
      }
      SDL_RenderPresent(gRenderer);

      if (delta < timePerFrame)
      {
        SDL_Delay(timePerFrame - delta);
      }
      startTime = endTime;
      endTime = SDL_GetTicks();
    }

    if (gameState.mapSize > 0)
    {
      for (int i = 0; i < gameState.mapSize; i++)
      {
        delete[] gameState.map[i];
      }
      delete[] gameState.map;
    }

    if (gameState.scoresLen > 0)
    {
      delete[] gameState.scores;
    }
  }
  close();
  return 0;
}
