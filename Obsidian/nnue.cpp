#include "nnue.h"
#include "bitboard.h"
#include "incbin.h"

#include <iostream>
#include <fstream>

INCBIN(EmbeddedNNUE, EvalFile);

namespace NNUE {

  constexpr int WeightsPerVec = sizeof(SIMD::Vec) / sizeof(weight_t);

  alignas(SIMD::Alignment) int FeaturesIndex[COLOR_NB][PIECE_NB][SQUARE_NB];

  struct {
    alignas(SIMD::Alignment) weight_t FeatureWeights[FeaturesWidth * HiddenWidth];
    alignas(SIMD::Alignment) weight_t FeatureBiases[HiddenWidth];
    alignas(SIMD::Alignment) weight_t OutputWeights[2 * HiddenWidth];
                             weight_t OutputBias;
  } Content;

  template <int InputSize>
  inline void addAll(weight_t* output, weight_t* input, int add0){
    Vec* inputVec = (Vec*)input;
    Vec* outputVec = (Vec*)output;
    Vec* add0Vec = (Vec*) &Content.FeatureWeights[add0];

    for (int i = 0; i < InputSize / WeightsPerVec; ++i)
      outputVec[i] = addEpi16(inputVec[i], add0Vec[i]);
  }

  template <int InputSize>
  inline void subAll(weight_t* output, weight_t* input, int sub0){
    Vec* inputVec = (Vec*)input;
    Vec* outputVec = (Vec*)output;
    Vec* sub0Vec = (Vec*) &Content.FeatureWeights[sub0];

    for (int i = 0; i < InputSize / WeightsPerVec; ++i)
      outputVec[i] = subEpi16(inputVec[i], sub0Vec[i]);
  }

  template <int InputSize>
  inline void addSubAll(weight_t* output, weight_t* input, int add0, int sub0) {
    Vec* inputVec = (Vec*)input;
    Vec* outputVec = (Vec*)output;

    Vec* add0Vec = (Vec*) &Content.FeatureWeights[add0];
    Vec* sub0Vec = (Vec*) &Content.FeatureWeights[sub0];

    for (int i = 0; i < InputSize / WeightsPerVec; ++i)
      outputVec[i] = subEpi16(addEpi16(inputVec[i], add0Vec[i]), sub0Vec[i]);
  }

  template <int InputSize>
  inline void subAddSub(weight_t* output, weight_t* input, int sub0, int add0, int sub1) {
    Vec* inputVec = (Vec*)input;
    Vec* outputVec = (Vec*)output;

    Vec* sub0Vec = (Vec*) &Content.FeatureWeights[sub0];
    Vec* add0Vec = (Vec*) &Content.FeatureWeights[add0];
    Vec* sub1Vec = (Vec*) &Content.FeatureWeights[sub1];

    for (int i = 0; i < InputSize / WeightsPerVec; ++i)
      outputVec[i] = subEpi16(subEpi16(addEpi16(inputVec[i], add0Vec[i]), sub0Vec[i]), sub1Vec[i]);
  }

   template <int InputSize>
  inline void subAddSubAdd(weight_t* output, weight_t* input, int sub0, int add0, int sub1, int add1) {
    Vec* inputVec = (Vec*)input;
    Vec* outputVec = (Vec*)output;

    Vec* sub0Vec = (Vec*) &Content.FeatureWeights[sub0];
    Vec* add0Vec = (Vec*) &Content.FeatureWeights[add0];
    Vec* sub1Vec = (Vec*) &Content.FeatureWeights[sub1];
    Vec* add1Vec = (Vec*) &Content.FeatureWeights[add1];

    for (int i = 0; i < InputSize / WeightsPerVec; ++i)
      outputVec[i] = addEpi16(subEpi16(subEpi16(addEpi16(inputVec[i], add0Vec[i]), sub0Vec[i]), sub1Vec[i]), add1Vec[i]);
  }

  void Accumulator::reset() {
    for (int c = WHITE; c <= BLACK; ++c)
      memcpy(colors[c], Content.FeatureBiases, sizeof(Content.FeatureBiases));
  }

  void Accumulator::activateFeature(Square sq, Piece pc, Accumulator* input) {
    for (int c = WHITE; c <= BLACK; ++c)
      addAll<HiddenWidth>(colors[c], input->colors[c], FeaturesIndex[c][pc][sq]);
  }

  void Accumulator::deactivateFeature(Square sq, Piece pc, Accumulator* input) {
    for (int c = WHITE; c <= BLACK; ++c)
      subAll<HiddenWidth>(colors[c], input->colors[c], FeaturesIndex[c][pc][sq]);
  }

  void Accumulator::moveFeature(Square from, Square to, Piece pc, Accumulator* input) {
    for (int c = WHITE; c <= BLACK; ++c)
      addSubAll<HiddenWidth>(colors[c], input->colors[c], FeaturesIndex[c][pc][to], FeaturesIndex[c][pc][from]);
  }

  inline int ftIndex(int color, SquarePiece sp) {
    return FeaturesIndex[color][sp.pc][sp.sq];
  }

  NNUE::Accumulator crazyTable[12][SQUARE_NB][SQUARE_NB];

  int crazyIdx(Piece pc) {
    if (colorOf(pc) == WHITE)
      return pc-1;
    else
      return pc-3;
  }

  void Accumulator::doUpdates(DirtyPieces* dp, Accumulator* input) {
    if (dp->type == DirtyPieces::CASTLING) {

      Vec* inputVec = (Vec*) input->colors;
      Vec* outputVec = (Vec*) this->colors;
      Vec* add0Vec = (Vec*) crazyTable[crazyIdx(dp->add0.pc)][dp->sub0.sq][dp->add0.sq].colors;
      Vec* add1Vec = (Vec*) crazyTable[crazyIdx(dp->add1.pc)][dp->sub1.sq][dp->add1.sq].colors;

      for (int i = 0; i < 2 * HiddenWidth / WeightsPerVec; ++i)
        outputVec[i] = addEpi16(inputVec[i], addEpi16(add0Vec[i], add1Vec[i]));

    } else if (dp->type == DirtyPieces::CAPTURE) {

      if (dp->add0.pc == dp->sub0.pc) {
        Vec* inputVec = (Vec*) input->colors;
        Vec* outputVec = (Vec*) this->colors;
        Vec* add0Vec = (Vec*) crazyTable[crazyIdx(dp->add0.pc)][dp->sub0.sq][dp->add0.sq].colors;

        for (int i = 0; i < 2 * HiddenWidth / WeightsPerVec; ++i)
          outputVec[i] = addEpi16(inputVec[i], add0Vec[i]);

        for (int c = WHITE; c <= BLACK; ++c)
          subAll<HiddenWidth>(colors[c], colors[c], ftIndex(c, dp->sub1));
      } else {
        for (int c = WHITE; c <= BLACK; ++c)
          subAddSub<HiddenWidth>(colors[c], input->colors[c], 
            ftIndex(c, dp->sub0), ftIndex(c, dp->add0), ftIndex(c, dp->sub1));
      }

    } else {
      if (dp->add0.pc == dp->sub0.pc) {
        Vec* inputVec = (Vec*) input->colors;
        Vec* outputVec = (Vec*) this->colors;
        Vec* add0Vec = (Vec*) crazyTable[crazyIdx(dp->add0.pc)][dp->sub0.sq][dp->add0.sq].colors;

        for (int i = 0; i < 2 * HiddenWidth / WeightsPerVec; ++i)
          outputVec[i] = addEpi16(inputVec[i], add0Vec[i]);
      } else {
        for (int c = WHITE; c <= BLACK; ++c)
          addSubAll<HiddenWidth>(colors[c], input->colors[c], 
            ftIndex(c, dp->add0), ftIndex(c, dp->sub0));
      }
    }
  }

  void init() {

    memcpy(&Content, gEmbeddedNNUEData, sizeof(Content));

    // Cache feature indexes
    for (int c = 0; c <= 1; ++c) {
      for (int pt = PAWN; pt <= KING; ++pt) {
        for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq) {
          Color color = Color(c);
          Piece piece = make_piece(color, PieceType(pt));

          FeaturesIndex[color][piece][sq] =
            SQUARE_NB * (pt - 1) + relative_square(color, sq);

          FeaturesIndex[~color][piece][sq] =
            SQUARE_NB * (pt + 5) + relative_square(~color, sq);

          FeaturesIndex[color][piece][sq] *= HiddenWidth;
          FeaturesIndex[~color][piece][sq] *= HiddenWidth;
        }
      }
    }

    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
      for (Color color : {WHITE, BLACK}) {

        Piece piece = make_piece(color, pt);

        for (Square s1 = SQ_A1; s1 < SQUARE_NB; ++s1) {
          for (Square s2 = SQ_A1; s2 < SQUARE_NB; ++s2) {
            Accumulator* target = & crazyTable[crazyIdx(piece)][s1][s2];
            memset(target, 0, sizeof(Accumulator));

            target->moveFeature(s1, s2, piece, target);
          }
        }
      }
    }
  }

  Score evaluate(Accumulator& accumulator, Color sideToMove) {

    Vec* stmAcc = (Vec*) accumulator.colors[sideToMove];
    Vec* oppAcc = (Vec*) accumulator.colors[~sideToMove];

    Vec* stmWeights = (Vec*) &Content.OutputWeights[0];
    Vec* oppWeights = (Vec*) &Content.OutputWeights[HiddenWidth];

    const Vec vecZero = vecSetZero();
    const Vec vecQA = vecSet1Epi16(NetworkQA);

    Vec sum = vecSetZero();
    Vec reg;

    for (int i = 0; i < HiddenWidth / WeightsPerVec; ++i) {
      // Side to move
      reg = maxEpi16(stmAcc[i], vecZero); // clip
      reg = minEpi16(reg, vecQA); // clip
      reg = mulloEpi16(reg, reg); // square
      reg = maddEpi16(reg, stmWeights[i]); // multiply with output layer
      sum = addEpi32(sum, reg); // collect the result

      // Non side to move
      reg = maxEpi16(oppAcc[i], vecZero);
      reg = minEpi16(reg, vecQA);
      reg = mulloEpi16(reg, reg);
      reg = maddEpi16(reg, oppWeights[i]);
      sum = addEpi32(sum, reg);
    }

    int unsquared = vecHaddEpi32(sum) / NetworkQA + Content.OutputBias;

    return Score((unsquared * NetworkScale) / NetworkQAB);
  }

}
