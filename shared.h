#pragma once

struct WaitingState
{
  char gamePhase;
  char playersConnected;
  char playersReady;
};

struct InGameState
{
  char gamePhase;
  char activeWorm;
  int score;
};

struct ScoreboardState
{
  char gamePhase;
  char players;
};

struct ScoreboardRecord
{
  char ind;
  int score;
};

enum GamePhases
{
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  SCOREBOARD
};