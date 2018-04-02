#include "shared.h"
#define REROLL_NUMBER_DICE_THRESHOLD 3
#define REROLL_HEALTH_THRESHOLD 5
#define RETREAT_HEALTH_THRESHOLD 5

/**
* Decides whether to reroll and sends the hub the appropriate message.
*   - game, a struct of the game state
*   - players, an array of players
*   - rollFile, the roll file containing the latest rolls
*/
void reroll(Game* game, Player** players, RollFile* rollFile) {
    if (rollFile->latestDice->numberOfOnes < REROLL_NUMBER_DICE_THRESHOLD) {
        rollFile->rerollDice->numberOfOnes = 
                rollFile->latestDice->numberOfOnes;
    }
    if (rollFile->latestDice->numberOfTwos < REROLL_NUMBER_DICE_THRESHOLD) {
        rollFile->rerollDice->numberOfTwos = 
                rollFile->latestDice->numberOfTwos;
    }
    if (rollFile->latestDice->numberOfThrees < REROLL_NUMBER_DICE_THRESHOLD) {
        rollFile->rerollDice->numberOfThrees = 
                rollFile->latestDice->numberOfThrees;
    }
    if (players[game->currentPlayerNumber]->health > 
            REROLL_HEALTH_THRESHOLD) {
        rollFile->rerollDice->numberOfHs = rollFile->latestDice->numberOfHs;
    }
    rollFile->rerollDice->numberOfAs = rollFile->latestDice->numberOfAs;
    rollFile->rerollDice->numberOfPs = rollFile->latestDice->numberOfPs;

    if (sum_dice_set(rollFile->rerollDice) == 0) {
        act_on_dice(game, players);
    } else {
        create_dice_set_string(rollFile->rerollDice);
        fprintf(stdout, "reroll %s\n", rollFile->rerollDice->rollString);
        fflush(stdout);
    }
}

/**
* Decides whether to retreat after being attacked. Returns true if so,
* otherwise returns false.
*   - game, a struct of the game state
*   - players, an array of players
*/
bool retreat(Game* game, Player** players) {
    if (players[game->currentPlayerNumber]->health < 
            RETREAT_HEALTH_THRESHOLD) {
        return true;
    }
    return false;
}
