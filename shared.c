#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "shared.h"

/**
* Initialises the game struct.
*   - game, a struct of the game state
*/
void initialise_game(Game* game) {
    game->playerInStLucia = EMPTY_STLUCIA; 
    game->numberOfRerolls = 0;

    game->rollFile = malloc(sizeof(RollFile));

    game->rollFile->latestDice = malloc(sizeof(DiceSet));
    game->rollFile->latestDice->rollString = 
            malloc(sizeof(char) * (DICE_SET_SIZE + 1));
    reset_dice_set(game->rollFile->latestDice);

    game->rollFile->rerollDice = 
            malloc(sizeof(DiceSet));
    game->rollFile->rerollDice->rollString = 
            malloc(sizeof(char) * (DICE_SET_SIZE + 1));
    reset_dice_set(game->rollFile->rerollDice);

    game->rollFile->oppositionDice = malloc(sizeof(DiceSet));
    game->rollFile->oppositionDice->rollString = 
            malloc(sizeof(char) * (DICE_SET_SIZE + 1));
    reset_dice_set(game->rollFile->oppositionDice);
}

/**
* Resets the specified dice set to 0s.
*   - diceSet, the diceSet to reset
*/
void reset_dice_set(DiceSet* diceSet) {
    diceSet->numberOfOnes = 0;
    diceSet->numberOfTwos = 0;
    diceSet->numberOfThrees = 0;
    diceSet->numberOfHs = 0;
    diceSet->numberOfAs = 0;
    diceSet->numberOfPs = 0;
}

/**
* Returns the sum of the specified dice set.
*   - diceSet, the diceSet to reset
*/
int sum_dice_set(DiceSet* diceSet) {
    return diceSet->numberOfOnes + diceSet->numberOfTwos +
            diceSet->numberOfThrees + diceSet->numberOfHs +
            diceSet->numberOfAs + diceSet->numberOfPs;
}

/**
* Initialises the players array.
*   - game, a struct of the game state
*   - players, an array of players
*/
void initialise_players(Game* game, Player** players) {
    for (int i = 0; i < game->numberOfPlayers; ++i) {
        players[i] = malloc(sizeof(Player));
        players[i]->health = STARTING_HEALTH;
        players[i]->tokens = 0;
        players[i]->points = 0;
        players[i]->status = UNCONNECTED;
        players[i]->inbox = NULL;
    }
}

/**
* Returns the integer of the player label.
*   - playerLabel, the label of the player 
*/
int get_player_number(char playerLabel) {
    return (int)(playerLabel - FIRST_PLAYER_LETTER);
}

/**
* Returns the label of the player number specified.
*   - playerLabel, the label of the player
*/
char get_player_label(int playerNumber) {
    char startingLabel = FIRST_PLAYER_LETTER;
    return (char)(startingLabel + playerNumber);
}

/**
* Returns the next die roll from the roll file.
*   - rollFile, the roll file 
*/
char get_next_die(RollFile* rollFile) {
    char nextDie = rollFile->diceRolls[rollFile->index];
    rollFile->index = (rollFile->index + 1) % rollFile->size;
    return nextDie;
}

/**
* Updates the dice set's tally for a particulat result, by the amount 
* specified.
*   - diceSet, the DiceSet to update
*   - die, the die to update
*   - updateAmount, the amount by which to change the die in diceSet
*/
void update_dice_set(DiceSet* diceSet, char die, int updateAmount) {
    switch (die) {
        case '1':
            diceSet->numberOfOnes += updateAmount;
            break;
        case '2':
            diceSet->numberOfTwos += updateAmount;
            break;
        case '3':
            diceSet->numberOfThrees += updateAmount;
            break;
        case 'H':
            diceSet->numberOfHs += updateAmount;
            break;
        case 'A':
            diceSet->numberOfAs += updateAmount;
            break;
        case 'P':
            diceSet->numberOfPs += updateAmount;
            break;
    }
}

/**
* Adds the specified die to the specified dice set.
*   - diceSet, the DiceSet to update
*   - die, the die to update
*/
void add_die_to_dice_set(DiceSet* diceSet, char die) {
    update_dice_set(diceSet, die, 1);
}


/**
* Subtracts the specified die from the specified dice set.
*   - diceSet, the DiceSet to update
*   - die, the die to update
*/
void remove_die_from_dice_set(DiceSet* diceSet, char die) {
    update_dice_set(diceSet, die, -1);
}


/**
* Adds dice from the roll file to the specified dice set.
*   - rollFile, the roll file to use
*   - numberOfDice, the number of dice to add
*   - diceSet, the DiceSet to update
*/
void add_dice_to_dice_set(RollFile* rollFile, int numberOfDice, 
        DiceSet* diceSet) {
    for (int i; i < numberOfDice; i++) {
        add_die_to_dice_set(diceSet, get_next_die(rollFile));
    }
}

/**
* Adds the die specified to a string of dice rolls, from the specified 
* position in that string, the number of times specified.
*   - diceString, the string to add the dice to
*   - numberOfDice, the number of dice to add
*   - valueToAdd, the die to add
*   - index, the position in the string to start adding dice
*/
void add_die_type(char* diceString, int numberOfDice, char valueToAdd, 
        int* index) {
    for (int i = 0; i < numberOfDice; i++) {
        diceString[*index] = valueToAdd;
        *index += 1;
    }
}

/**
* Creates a string of the specified diceSet
*   - diceSet, the DiceSet to generate the string from 
*/
void create_dice_set_string(DiceSet* diceSet) {
    memset(diceSet->rollString, '\0', DICE_SET_SIZE + 1);
    diceSet->rollString[DICE_SET_SIZE] = '\0';
    int index = 0;

    add_die_type(diceSet->rollString, 
            diceSet->numberOfOnes, '1', &index);
    add_die_type(diceSet->rollString, 
            diceSet->numberOfTwos, '2', &index);
    add_die_type(diceSet->rollString, 
            diceSet->numberOfThrees, '3', &index);
    add_die_type(diceSet->rollString, 
            diceSet->numberOfHs, 'H', &index);
    add_die_type(diceSet->rollString, 
            diceSet->numberOfAs, 'A', &index);
    add_die_type(diceSet->rollString, 
            diceSet->numberOfPs, 'P', &index);
    add_die_type(diceSet->rollString, 1, '\0', &index);
}

/**
* Converts a string to an array of commands, using spaces as the delimiter. 
* Returns the number of commands, or -1 if it fails.
*   - message, the message to convert 
*   - commands, the array of commands generatedf 
*/
int interpret_message(char* message, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH]) {
    int messageIndex = 0;
    int currentCommand = 0;
    int currentCommandPosition = 0;
    while (true) {
        char current = (char)message[messageIndex];
        if (current == ' ') {
            commands[currentCommand][currentCommandPosition] = '\0';
            messageIndex++;
            currentCommand++;
            currentCommandPosition = 0;
            continue;
        } else if (current == '\0' || current == '\n') {
            commands[currentCommand][currentCommandPosition] = '\0';
            currentCommand++;
            currentCommandPosition = 0;
            return currentCommand;
        }
        commands[currentCommand][currentCommandPosition] = current;
        messageIndex++;
        currentCommandPosition++;
    }
    return -1;
}

/**
* Heals the specified player by the specified amount 
*   - player, the player to heal
*   - game, a struct of the game state
*   - players, an array of players
*   - isHub, a boolean which if true will broadcast the heal to the players
*   - healAmount, the amount to heal by
*/
void heal(int player, Game* game, Player** players,
        bool isHub, int healAmount) {
    int recover = 0;
    if (healAmount == 0) {
        return;
    }
    if (player != game->playerInStLucia) {
        if (players[player]->health + healAmount > STARTING_HEALTH) {
            recover = STARTING_HEALTH - players[player]->health;
        } else {
            recover = healAmount;
        }
        players[player]->health += recover;
    } else {
        return;
    }
    if (isHub) {
        fprintf(stderr, "Player %c healed %d, health is now %d\n", 
                get_player_label(player), recover, 
                players[player]->health);
    }
}

/**
* Damages the specified player by the specified amount
*   - player, the player to damage
*   - damage, the amount of damage
*   - game, a struct of the game state
*   - players, an array of players
*   - isHub, a boolean which if true will broadcast the damage to the players
*/
void damage_player(int player, int damage, Game* game, Player** players, 
        bool isHub) {
    int healthReduction = 0;

    if (players[player]->health - damage < 0) {
        healthReduction = players[player]->health;
    } else {
        healthReduction = damage;
    }
    players[player]->health -= healthReduction;

    if (isHub) {
        fprintf(stderr, "Player %c took %d damage, health is now %d\n", 
                get_player_label(player), healthReduction, 
                players[player]->health);
    }
}

/**
* Returns the number of players not eliminated
*   - game, a struct of the game state
*   - players, an array of players
*/
int players_remaining(Game* game, Player** players) {
    int tally = 0;
    for (int i = 0; i < game->numberOfPlayers; i++) {
        if (players[i]->status != ELIMINATED) {
            tally++;
        }
    }
    return tally;
}

/**
* Checks if the provided dice roll contains invalid characters. Returns true
* if it does otherwise returns false.
*   - message, the dice roll string being validated
*/
bool invalid_roll(char* message) {
    for (int i = 0; i < strlen(message); i++) {
        char c = message[i];
        if (c != DICE_CHARACTER_1 && c != DICE_CHARACTER_2 &&
                c != DICE_CHARACTER_3 && c != DICE_CHARACTER_4 &&
                c != DICE_CHARACTER_5 && c != DICE_CHARACTER_6) {
            return true;
        }
    }
    return false;
}

/**
* Frees the memory allocated previously with malloc.
*   - game, a struct of the game state
*   - players, an array of players
*/
void free_allocated_memory(Game* game, Player** players) {

    switch (game->mallocProgress) {
        case ROLL_FILE:
            free(game->rollFile->diceRolls);
        case PLAYERS:
            for (int i = 0; i < game->numberOfPlayers; ++i) {
                free(players[i]);
            }
            free(players);
        case GAME:
            free(game->rollFile->oppositionDice->rollString);
            free(game->rollFile->oppositionDice);
            free(game->rollFile->rerollDice->rollString);
            free(game->rollFile->rerollDice);
            free(game->rollFile->latestDice->rollString);
            free(game->rollFile->latestDice);
            free(game->rollFile);
            free(game);
        default:
            break;
    }
}
