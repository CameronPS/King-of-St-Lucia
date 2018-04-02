#include "shared.h"

/**
* Decides whether to reroll and sends the hub the appropriate message.
*   - game, a struct of the game state
*   - players, an array of players
*   - rollFile, the roll file containing the latest rolls
*/
void reroll(Game* game, Player** players, RollFile* rollFile) {
    if (game->currentPlayerNumber == game->playerInStLucia) {
        rollFile->rerollDice->numberOfAs = rollFile->latestDice->numberOfAs;
    } else {
        rollFile->rerollDice->numberOfPs = rollFile->latestDice->numberOfPs;
        if ((game->playerInStLucia != EMPTY_STLUCIA) && 
                (rollFile->latestDice->numberOfAs >= 
                players[game->playerInStLucia]->health)) {
            /* Do not reroll 'A's if sufficient to kill player in St Lucia */
        } else {
            rollFile->rerollDice->numberOfAs = 
                    rollFile->latestDice->numberOfAs;
        }
    }
    rollFile->rerollDice->numberOfOnes = rollFile->latestDice->numberOfOnes;
    rollFile->rerollDice->numberOfTwos = rollFile->latestDice->numberOfTwos;
    rollFile->rerollDice->numberOfThrees = 
            rollFile->latestDice->numberOfThrees;
    rollFile->rerollDice->numberOfHs = rollFile->latestDice->numberOfHs;

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
    return false;
}
