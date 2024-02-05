#pragma once

#include "history.h"
#include "nnue.h"
#include "position.h"
#include "types.h"

#include <condition_variable>
#include <vector>

namespace Search {

  extern bool doingBench;

  struct Settings {

    clock_t time[COLOR_NB], inc[COLOR_NB], movetime, startTime;
    int movestogo, depth;
    int64_t nodes;

    Position position;

    std::vector<uint64_t> prevPositions;

    Settings() {
      time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = movetime = 0;
      movestogo = 0;
      depth = MAX_PLY-4; // no depth limit by default
      nodes = 0;
    }

    inline bool hasTimeLimit() const {
      return time[WHITE] || time[BLACK];
    }
  };

  struct SearchLoopInfo {
    Score score;
    Move bestMove;
  };

  struct SearchInfo {
    Score staticEval;
    Move playedMove;

    Move killerMove;

    Move pv[MAX_PLY];
    int pvLength;

    int doubleExt;

    Move excludedMove;

    PieceToHistory* mContHistory;

    PieceToHistory& contHistory() {
      return *mContHistory;
    }
  };

  // A sort of header of the search stack, so that plies behind 0 are accessible and
  // it's easier to determine conthist score, improving, ...
  constexpr int SsOffset = 4;

  class Thread {

  public:

    std::mutex mutex;
    std::condition_variable cv;

    volatile bool searching = false;
    volatile bool exitThread = false;
    std::thread thread;

    uint64_t nodesSearched;
    uint64_t tbHits;

    Thread();

    void resetHistories();
    
  private:
    
    clock_t optimumTime, maximumTime;

    int rootDepth;

    int ply = 0;

    int keyStackHead;
    Key keyStack[100 + MAX_PLY];

    int accumStackHead;
    NNUE::Accumulator accumStack[MAX_PLY];

    SearchInfo searchStack[MAX_PLY + SsOffset];

    MoveList rootMoves;
    int rootMovesNodes[MAX_MOVES];

    MainHistory mainHistory;
    CaptureHistory captureHistory;
    ContinuationHistory contHistory;
    CounterMoveHistory counterMoveHistory;

    bool usedMostOfTime();

    void playNullMove(Position& pos, SearchInfo* ss);

    void cancelNullMove();

    void playMove(Position& pos, Move move, SearchInfo* ss);

    void cancelMove();

    int getQuietHistory(Position& pos, Move move, SearchInfo* ss);

    int getCapHistory(Position& pos, Move move);

    void updateHistories(Position& pos, int depth, Move bestMove, Score bestScore,
      Score beta, Move* quietMoves, int quietCount, SearchInfo* ss);

    bool hasUpcomingRepetition(Position& pos, int ply);

    // Should not be called from Root node
    bool isRepetition(Position& pos, int ply);

    Score makeDrawScore();

    template<bool IsPV>
    Score qsearch(Position& position, Score alpha, Score beta, SearchInfo* ss);

    template<bool IsPV>
    Score negamax(Position& position, Score alpha, Score beta, int depth, bool cutNode, SearchInfo* ss);

    void startSearch();

    void idleLoop();
  };

  template<bool root>
  int64_t perft(Position& pos, int depth);

  void initLmrTable();

  void init();
}