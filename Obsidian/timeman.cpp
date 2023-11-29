#include "timeman.h"
#include "search.h"
#include "uci.h"

#include <cfloat>
#include <cmath>

namespace TimeMan {

  /// Calcaulate how much of our time we should use.
  // Its important both to play good moves and to not run out of clock

  clock_t calcOptimumTime(Search::Settings& settings, Color us) {

    // optScale is a percentage of available time to use for the current move.
    double optScale;

    int mtg = settings.movestogo ? std::min(settings.movestogo, 50) : 50;

    clock_t timeLeft = std::max(clock_t(1),
      settings.time[us] + settings.inc[us] * (mtg - 1) - 10 * (2 + mtg));

    if (settings.movestogo == 0) {
      optScale = std::min(0.024,
        0.2 * settings.time[us] / double(timeLeft));
    }
    else {
      optScale = std::min(0.966 / mtg,
        0.88 * settings.time[us] / double(timeLeft));
    }

    return clock_t(optScale * timeLeft);
  }
}