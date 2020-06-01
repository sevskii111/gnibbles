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

struct ScoreBoardState
{
  char gamePhase;
  char activeWorm;
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