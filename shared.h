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
  char score;
};

struct ScoreBoardState
{
  char gamePhase;
  char activeWorm;
};

enum GamePhases
{
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  SCOREBOARD
};