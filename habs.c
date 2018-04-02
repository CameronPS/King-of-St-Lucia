#include "shared.h"
#define REROLL_HEALTH_THRESHOLD 5
#define RETREAT_HEALTH_THRESHOLD 4
#define RETREAT_REMAINING_PLAYERS_THRESHOLD 2

/**
* Decides whether to reroll and sends the hub the appropriate message.
*   - game, a struct of the game state
*   - players, an array of players
*   - rollFile, the roll file containing the latest rolls
*/
void reroll(Game* game, Player** players, RollFile* rollFile) {
    if (players[game->currentPlayerNumber]->health < 
            REROLL_HEALTH_THRESHOLD) {
        rollFile->rerollDice->numberOfAs = rollFile->latestDice->numberOfAs;
    }
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
    if (players_remaining(game, players) == 
            RETREAT_REMAINING_PLAYERS_THRESHOLD) {
        return false;
    }
    if (players[game->currentPlayerNumber]->health <
            RETREAT_HEALTH_THRESHOLD) {
        return true;
    }
    return false;
}
