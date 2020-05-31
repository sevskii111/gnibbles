struct GlobalState
{
  char gamePhase;
  char playersConnected;
  char playersReady;
  char activeWorm;
};

enum GamePhases
{
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  SCOREBOARD
};