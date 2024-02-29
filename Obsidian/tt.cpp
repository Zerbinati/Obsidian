#include "tt.h"

#include <iostream>

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace TT {

  constexpr size_t Mega = 1024 * 1024;

  Entry* entries = nullptr;
  uint64_t entryCount;

  void clear() {
    memset(entries, 0, sizeof(Entry) * entryCount);
  }

  void resize(size_t megaBytes) {
    if (entries)
      free(entries);

    size_t bytes = megaBytes * Mega;
    entryCount = bytes / sizeof(Entry);

#if defined(__linux__)
    entries = (Entry*) aligned_alloc(2 * Mega, bytes);
    madvise(entries, bytes, MADV_HUGEPAGE);
#else
    exit(-1);
#endif 

    clear();
  }

  uint64_t index(Key key) {
    using uint128 = unsigned __int128;
    return (uint128(key) * uint128(entryCount)) >> 64;
  }

  void prefetch(Key key) {
#if defined(_MSC_VER)
    _mm_prefetch((char*)&entries[index(key)], _MM_HINT_T0);
#else
    __builtin_prefetch(&entries[index(key)]);
#endif
  }

  Entry* probe(Key key, bool& hit) {
    Entry* entry = &entries[index(key)];
    hit = entry->matches(key);
    return entry;
  }

  void Entry::store(Key _key, Flag _bound, int _depth, Move _move, Score _score, Score _eval, bool isPV, int ply) {
    if ( _bound == FLAG_EXACT
      || !matches(_key)
      || _depth + 4 + 2*isPV > this->depth) {

        if (_score >= SCORE_TB_WIN_IN_MAX_PLY)
          _score += ply;
        else if (score <= SCORE_TB_LOSS_IN_MAX_PLY)
          _score -= ply;

        if (!matches(_key) || _move)
          this->move = _move;

        this->key32 = (uint32_t) _key;
        this->depth = _depth;
        this->score = _score;
        this->staticEval = _eval;
        this->flags = _bound;
        if (isPV)
          this->flags |= FLAG_PV;
      }
  }
}