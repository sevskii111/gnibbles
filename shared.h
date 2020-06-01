#define MAX_MAP_SIZE 128

struct WaitingState{
  char gamePhase;
  char playersConnected;
  char playersReady;
};

struct InGameState{
  char gamePhase;
  char activeWorm;
};

struct ScoreBoardState{
  char gamePhase;
  char activeWorm;
};

enum GamePhases
{
  WAITING_FOR_PLAYERS,
  IN_PROGRESS,
  SCOREBOARD
};