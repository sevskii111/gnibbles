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
};

struct ScoreBoardState
{
  char gamePhase;
  char activeWorm;
  char score;
};

enum GamePhases
{
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  SCOREBOARD
};