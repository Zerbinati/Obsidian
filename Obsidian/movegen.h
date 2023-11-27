#pragma once

#include "move.h"
#include "position.h"

/*
* All legal moves, plus ones that could make the side to move fall in check
*/
void getPseudoLegalMoves(const Position& pos, MoveList* moveList);

void getStageMoves(const Position& pos, bool quiets, MoveList* moveList);