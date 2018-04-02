#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "shared.h"

/* Game constants */
#define ALLOWED_REROLLS 2

/* Format properties of the player arguments, for error
checking */
#define PLAYER_ARGUMENT_COUNT 3
#define LABEL_ARGUMENT_INDEX 2
#define PLAYER_COUNT_ARGUMENT_INDEX 1

/* Format properties of all the messages the player can receive, for error
checking */
#define TURN_MESSAGE_SIZE 2
#define TURN_DICE_ROLL_INDEX 1
#define BASE_FOR_INTEGER_CONVERSION 10

#define REROLLED_MESSAGE_SIZE 2
#define REROLLED_DICE_ROLL_INDEX 1

#define ROLLED_MESSAGE_SIZE 3
#define ROLLED_PLAYER_LABEL_INDEX 1
#define ROLLED_DICE_ROLL_INDEX 2

#define POINTS_MESSAGE_SIZE 3
#define POINTS_PLAYER_LABEL_INDEX 1
#define POINTS_VALUE_INDEX 2
#define POINTS_VALUE_SIZE 1

#define ATTACKS_MESSAGE_SIZE 4
#define ATTACKS_PLAYER_LABEL_INDEX 1
#define ATTACKS_VALUE_INDEX 2
#define ATTACKS_VALUE_SIZE 1
#define ATTACKS_MIN '0'
#define ATTACKS_MAX '6'
#define ATTACKS_DIRECTION_INDEX 3

#define ELIMINATED_MESSAGE_SIZE 2
#define ELIMINATED_PLAYER_LABEL_INDEX 1

#define CLAIM_MESSAGE_SIZE 2
#define CLAIM_PLAYER_LABEL_INDEX 1

#define STAY_MESSAGE_SIZE 1

#define WINNER_MESSAGE_SIZE 2
#define WINNER_PLAYER_LABEL_INDEX 1

#define SHUTDOWN_MESSAGE_SIZE 1

/**
* An enum for the different exit codes
*   - SUCCESS, normal exit due to game over
*   - INVALID_ARGUMENT_COUNT, wrong number of arguments
*   - INVALID_PLAYER_COUNT, invalid number of players 
*   - INVALID_ID, invalid player ID 
*   - PIPING_FAILURE, pipe from stlucia closed unexpectedly
*   - INVALID_MESSAGE, unexpectedly lost contact with StLucia
*/
typedef enum {
    SUCCESS = 0,
    INVALID_ARGUMENT_COUNT = 1,
    INVALID_PLAYER_COUNT = 2,
    INVALID_ID = 3,
    PIPING_FAILURE = 4,
    INVALID_MESSAGE = 5
} ExitCodes;

/**
* Exits the game, with the specified exit status and a message.
*   - game, a struct of the game state
*   - players, an array of players
*   - exitStatus, the exit status to exit with
*/
void exit_program(Game* game, Player** players, ExitCodes exitStatus) {
    free_allocated_memory(game, players);

    char* errorString;
    switch (exitStatus) {
        case SUCCESS:
            errorString = "";
            break;
        case INVALID_ARGUMENT_COUNT:
            errorString = "Usage: player number_of_players my_id\n";
            break;
        case INVALID_PLAYER_COUNT:
            errorString = "Invalid player count\n";
            break;
        case INVALID_ID:
            errorString = "Invalid player ID\n";
            break;
        case PIPING_FAILURE:
            errorString = "Unexpectedly lost contact with StLucia\n";
            break;
        case INVALID_MESSAGE:
            errorString = "Bad message from StLucia\n";
            break;
    }
    fprintf(stderr, "%s", errorString);
    exit((int)exitStatus);
}

/**
* Sends the hub "keepall", accepts the rolled dice and acts on the roll.
*   - game, a struct of the game state
*   - players, an array of players
*/
void act_on_dice(Game* game, Player** players) {
    fprintf(stdout, "keepall\n"); 
    fflush(stdout);
    heal(game->currentPlayerNumber, game, players, false, 
            game->rollFile->latestDice->numberOfHs);
}

/**
* Resets stored dice sets, stores the latest rolls, and rerolls if appropriate.
*   - roll, the roll string to use
*   - game, a struct of the game state
*   - players, an array of players
*/
void handle_turn(char* roll, Game* game, Player** players) {
    reset_dice_set(game->rollFile->latestDice);
    reset_dice_set(game->rollFile->rerollDice);

    for (int i = 0; i < DICE_SET_SIZE; i++) {
        add_die_to_dice_set(game->rollFile->latestDice, (char)roll[i]);
    }
    if (game->numberOfRerolls >= ALLOWED_REROLLS) {
        act_on_dice(game, players);
        return;
    }
    reroll(game, players, game->rollFile);
}

/**
* Checks that the provided player label is invalid. Returns true if it is 
* otherwise returns false.
*   - label, the player label being validated
*   - game, a struct of the game state
*/
bool invalid_label(char label, Game* game) {
    if (label < FIRST_PLAYER_LETTER || 
            label > get_player_label(game->numberOfPlayers - 1)) {
        return true;
    }
    return false;
}

/**
* Checks that the "turn..." message received is valid. Will exit program if 
* it is not. Otherwise it will handle the command.
*   - message, the "turn..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent 
*   - numberCommands, the number of commands sent
*/
void validate_turn(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strcmp(commands[0], "turn") == 0) {
        game->numberOfRerolls = 0;
        if (strlen(commands[TURN_DICE_ROLL_INDEX]) != DICE_SET_SIZE ||
                numberCommands != TURN_MESSAGE_SIZE ||
                invalid_roll(commands[TURN_DICE_ROLL_INDEX])) {
            exit_program(game, players, INVALID_MESSAGE);
        }
        handle_turn(commands[TURN_DICE_ROLL_INDEX], game, players);
    }
}

/**
* Checks that the "rerolled..." message received is valid. Will exit program 
* if it is not. Otherwise it will handle the command.
*   - message, the "rerolled..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_rerolled(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[REROLLED_DICE_ROLL_INDEX]) != DICE_SET_SIZE ||
            numberCommands != REROLLED_MESSAGE_SIZE ||
            invalid_roll(commands[REROLLED_DICE_ROLL_INDEX])) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    game->numberOfRerolls++;
    handle_turn(commands[REROLLED_DICE_ROLL_INDEX], game, players);
}

/**
* Checks that the "rolled..." message received is valid. Will exit program if 
* it is not. Otherwise it will handle the command.
*   - message, the "rolled..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_rolled(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[ROLLED_PLAYER_LABEL_INDEX]) != LABEL_LENGTH || 
            invalid_label(commands[ROLLED_PLAYER_LABEL_INDEX][0], game) ||
            strlen(commands[ROLLED_DICE_ROLL_INDEX]) != DICE_SET_SIZE || 
            numberCommands != ROLLED_MESSAGE_SIZE ||
            invalid_roll(commands[ROLLED_DICE_ROLL_INDEX])) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    int healing = 0;
    for (int i = 0; i < DICE_SET_SIZE; i++) {
        if (commands[ROLLED_DICE_ROLL_INDEX][i] == 'H') {
            healing++;
        }
    }
    heal(get_player_number(commands[ROLLED_PLAYER_LABEL_INDEX][0]), game, 
            players, false, healing);
}

/**
* Checks that the "points..." message received is valid. Will exit program if 
* it is not. Otherwise it will handle the command.
*   - message, the "points..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_points(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[POINTS_PLAYER_LABEL_INDEX]) != LABEL_LENGTH || 
            invalid_label(commands[POINTS_PLAYER_LABEL_INDEX][0], game) ||
            strlen(commands[POINTS_VALUE_INDEX]) != POINTS_VALUE_SIZE || 
            numberCommands != POINTS_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
}

/**
* Checks that the "attacks..." message received is valid. Will exit program if
* it is not. Otherwise it will handle the command.
*   - message, the "attacks..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_attacks(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[ATTACKS_PLAYER_LABEL_INDEX]) != LABEL_LENGTH ||
            invalid_label(commands[ATTACKS_PLAYER_LABEL_INDEX][0], game) ||
            strlen(commands[ATTACKS_VALUE_INDEX]) != ATTACKS_VALUE_SIZE ||
            numberCommands != ATTACKS_MESSAGE_SIZE ||
            commands[ATTACKS_VALUE_INDEX][0] < ATTACKS_MIN ||
            commands[ATTACKS_VALUE_INDEX][0] > ATTACKS_MAX) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    if (strcmp(commands[ATTACKS_DIRECTION_INDEX], "in") == 0) {
        if (game->playerInStLucia == EMPTY_STLUCIA) {
            return;
        }
        damage_player(game->playerInStLucia, 
                atoi(commands[ATTACKS_VALUE_INDEX]), game, players, false); 
    } else if (strcmp(commands[ATTACKS_DIRECTION_INDEX], "out") == 0) {
        for (int i = 0; i < game->numberOfPlayers; i++) {
            if (game->playerInStLucia == i) {
                continue;
            }
            damage_player(i, atoi(commands[ATTACKS_VALUE_INDEX]), 
                    game, players, false);
        }
    } else {
        exit_program(game, players, INVALID_MESSAGE);
    }
}

/**
* Checks that the "eliminated..." message received is valid. Will exit program
* if it is not. Otherwise it will handle the command.
*   - message, the "eliminated..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_eliminated(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[ELIMINATED_PLAYER_LABEL_INDEX]) != LABEL_LENGTH || 
            invalid_label(commands[ELIMINATED_PLAYER_LABEL_INDEX][0], game) ||
            numberCommands != ELIMINATED_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    int player = get_player_number(commands[ELIMINATED_PLAYER_LABEL_INDEX][0]);
    players[player]->status = ELIMINATED;
    if (player == game->currentPlayerNumber) {
        exit_program(game, players, SUCCESS);
    }
}

/**
* Checks that the "claim..." message received is valid. Will exit program
* if it is not. Otherwise it will handle the command.
*   - message, the "claim..."  message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_claim(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[CLAIM_PLAYER_LABEL_INDEX]) != LABEL_LENGTH || 
            invalid_label(commands[CLAIM_PLAYER_LABEL_INDEX][0], game) ||
            numberCommands != CLAIM_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    game->playerInStLucia = 
            get_player_number(commands[CLAIM_PLAYER_LABEL_INDEX][0]);
}

/**
* Checks that the "stay?" message received is valid. Will exit program
* if it is not. Otherwise it will handle the command.
*   - message, the "stay?" message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_stay(char* message, Game* game, Player** players,
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (numberCommands != STAY_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    if (retreat(game, players)) {
        fprintf(stdout, "go\n");
        fflush(stdout);
    } else {
        fprintf(stdout, "stay\n");
        fflush(stdout);
    }
}

/**
* Checks that the "winner..." message received is valid. Will exit program
* if it is not. Otherwise it will handle the command.
*   - message, the "winner..." message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_winner(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (strlen(commands[WINNER_PLAYER_LABEL_INDEX]) != LABEL_LENGTH || 
            invalid_label(commands[WINNER_PLAYER_LABEL_INDEX][0], game) ||
            numberCommands != WINNER_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    exit_program(game, players, SUCCESS);
}

/**
* Checks that the "shutdown" message received is valid. Will exit program. 
*   - message, the "shutdown" message being validated
*   - game, a struct of the game state
*   - players, an array of players
*   - commands, an array representation of the commands sent
*   - numberCommands, the number of commands sent
*/
void validate_shutdown(char* message, Game* game, Player** players, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH], int numberCommands) {
    if (numberCommands != SHUTDOWN_MESSAGE_SIZE) {
        exit_program(game, players, INVALID_MESSAGE);
    }
    exit_program(game, players, SUCCESS);
}

/**
* Checks that the message received from the hub is valid. Will exit program
* if it is not. Otherwise it will handle the command.
*   - message, the message received from the hub 
*   - game, a struct of the game state
*   - players, an array of players
*/
void handle_message(char* message, Game* game, Player** players) {
    fprintf(stderr, "From StLucia:%s", message);  
    char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH];
    int numberCommands = interpret_message(message, commands);

    if (strcmp(commands[0], "turn") == 0) {
        validate_turn(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "rerolled") == 0) {
        validate_rerolled(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "rolled") == 0) {
        validate_rolled(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "points") == 0) {
        validate_points(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "attacks") == 0) {
        validate_attacks(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "eliminated") == 0) {
        validate_eliminated(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "claim") == 0) {
        validate_claim(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "stay?") == 0) {
        validate_stay(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "winner") == 0) {
        validate_winner(message, game, players, commands, numberCommands);
    } else if (strcmp(commands[0], "shutdown") == 0) {
        validate_shutdown(message, game, players, commands, numberCommands);
    } else {
        exit_program(game, players, INVALID_MESSAGE);
    }
}

/**
* Starts a loop that waits for a message from the hub. If an error occurs this
* will exit the program. Otherwise it will attempt to handle the message.
* if it is not. Otherwise it will handle the command.
*   - game, a struct of the game state
*   - players, an array of players
*/
void initiate_response_loop(Game* game, Player** players) {
    char message[MAX_MESSAGE_LENGTH];
    char* error;
    while (true) {
        error = fgets(message, MAX_MESSAGE_LENGTH, stdin);
        if (error == NULL) {
            exit_program(game, players, PIPING_FAILURE);
        }
        handle_message(message, game, players);
    }
}

int main(int argc, char** argv) {
    //Define SIGINT and SIGPIPE handlers
    struct sigaction sigint;
    sigint.sa_handler = SIG_IGN;
    sigint.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigint, 0);

    struct sigaction sigpipe;
    sigpipe.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigpipe, 0);

    Game* game = malloc(sizeof(Game));
    initialise_game(game);
    game->mallocProgress = GAME;

    if (argc != PLAYER_ARGUMENT_COUNT) {
        exit_program(game, NULL, INVALID_ARGUMENT_COUNT);
    }

    char* playerCountError = 0;
    game->numberOfPlayers = 
            (int)strtoul(argv[PLAYER_COUNT_ARGUMENT_INDEX], 
            &playerCountError, BASE_FOR_INTEGER_CONVERSION);
    if ((*playerCountError != '\0') || 
            game->numberOfPlayers < MIN_PLAYERS ||
            game->numberOfPlayers > MAX_PLAYERS) {
        exit_program(game, NULL, INVALID_PLAYER_COUNT);
    }

    Player** players;
    players = malloc(sizeof(Player*) * game->numberOfPlayers);
    initialise_players(game, players);
    game->mallocProgress = PLAYERS;

    if (invalid_label(argv[LABEL_ARGUMENT_INDEX][0], game)) {
        exit_program(game, players, INVALID_ID);
    }
    game->currentPlayerNumber = 
            get_player_number(argv[LABEL_ARGUMENT_INDEX][0]);
    if (strlen(argv[LABEL_ARGUMENT_INDEX]) != 1) {
        exit_program(game, players, INVALID_ID);
    }
    fprintf(stdout, "!");
    fflush(stdout);

    initiate_response_loop(game, players);

    exit_program(game, players, SUCCESS);
    return 0; 
}

