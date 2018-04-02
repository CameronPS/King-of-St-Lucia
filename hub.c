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

/* Argument information for the St Lucia hub */
#define HUB_MIN_ARGS 5
#define HUB_MAX_ARGS 29
#define HUB_ARGUMENTS_NOT_PLAYERS 3
#define MAX_PLAYER_COUNT_STRING_SIZE 3
#define HUB_SCORE_LIMIT_ARGUMENT_INDEX 2

/* Constants used in generating pipes */
#define PIPE_SIZE 2
#define PIPE_INPUT 1
#define PIPE_OUTPUT 0

/* Constants used in calculating points */
#define STARTING_IN_STLUCIA_POINTS 2
#define TOKENS_POINTS_THRESHOLD 10
#define DICE_POINTS_THRESHOLD 2
#define ONES_DICE_POINT_PENALTY 2
#define TWOS_DICE_POINT_PENALTY 1
#define THREES_DICE_POINT_PENALTY 0

#define REROLLED_DICE_ROLL_INDEX 1

/* A global variable for the game, needed by the SIGINT handler. */
Game* game;

/* A global variable for the players, needed by the SIGINT handler. */
Player** players;

/** 
 * An enum for the different exit codes
 *   - SUCCESS, normal exit due to game over
 *   - INVALID_ARGUMENTS, wrong number of arguments
 *   - INVALID_SCORE, winscore is not a positive integer
 *   - OPEN_ERROR, unable to open rolls file for reading
 *   - INVALID_FILE, contents of the rolls file are invalid
 *   - PIPING_FAILURE, there was an error starting and piping to a player 
 *   process
 *   - PLAYER_QUIT, a player process ends unexpectedly
 *   - INVALID_MESSAGE, one of the players has sent an invalid message
 *   - INVALID_REQUEST, one of the players sent a properly formed message 
 *   but it was not a legal action
 *   - SIGINT_ACTION, hub received SIGINT	
 */
typedef enum {
    SUCCESS = 0,
    INVALID_ARGUMENTS = 1,
    INVALID_SCORE = 2,
    OPEN_ERROR = 3,
    INVALID_FILE = 4,
    PIPING_FAILURE = 5,
    PLAYER_QUIT = 6,
    INVALID_MESSAGE = 7,
    INVALID_REQUEST = 8,
    SIGINT_ACTION = 9
} ExitCodes;


/**
* Sends the message specified to all players. Will not send the message to
* eliminated players.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*   - message, the message to send
*   - skipActivePlayer, a boolean to be set true if the message is not to be
*       sent to the active player
*/
void alert_remaining_players(Game* game, Player** players, int activePlayer, 
        char* message, bool skipActivePlayer) {
    for (int i = 0; i < game->numberOfPlayers; i++) {
        if (players[i]->status != REMAINING) {
            continue;
        }
        if (activePlayer == i && skipActivePlayer) {
            continue;
        }
        if (players[i]->inbox == NULL) {
            continue;
        }
        fprintf(players[i]->inbox, message);
        fflush(players[i]->inbox);
    }
}

/**
* Closes the remaining players, by sending the shutdown command.
*   - game, a struct of the game state
*   - players, an array of players
*   - exitStatus, the exit status closing the game 
*/
void close_remaining_players(Game* game, Player** players, 
        ExitCodes exitStatus) {
    if (exitStatus >= PLAYER_QUIT || exitStatus == SUCCESS) {
        char shutDown[MAX_MESSAGE_LENGTH];
        sprintf(shutDown, "shutdown\n");
        alert_remaining_players(game, players, 0, shutDown, false);

        for (int i = 0; i < game->numberOfPlayers; i++) {
            int childStatus;
            alarm(2); // stop waiting after 2 seconds
            waitpid(players[i]->pid, &childStatus, 0);
            if (WIFEXITED(childStatus)) {
                if (WEXITSTATUS(childStatus)) {
                    fprintf(stderr, "Player %c exited with status %d\n", 
                            get_player_label(i), WEXITSTATUS(childStatus));
                } else {
                    /* Terminated with exit status 0, do nothing */
                }
            } else if (WIFSIGNALED(childStatus)) {
                kill(players[i]->pid, SIGKILL);
                fprintf(stderr, "Player %c terminated due to signal %d\n", 
                        get_player_label(i), WTERMSIG(childStatus));
            }
        }
    }
}

/**
* Exits the game, with the specified exit status and a message.
*   - game, a struct of the game state
*   - players, an array of players
*   - exitStatus, the status to exit with
*/
void exit_program(Game* game, Player** players, ExitCodes exitStatus) {
    close_remaining_players(game, players, exitStatus);
    free_allocated_memory(game, players);

    char* errorString;
    switch (exitStatus) {
        case SUCCESS:
            errorString = "";
            break;
        case INVALID_ARGUMENTS:
            errorString = "Usage: stlucia rollfile winscore "
                    "prog1 prog2 [prog3 [prog4]]\n";
            break;
        case INVALID_SCORE:
            errorString = "Invalid score\n";
            break;
        case OPEN_ERROR:
            errorString = "Unable to access rollfile\n";
            break;
        case INVALID_FILE:
            errorString = "Error reading rolls\n";
            break;
        case PIPING_FAILURE:
            errorString = "Unable to start subprocess\n";
            break;
        case PLAYER_QUIT:
            errorString = "Player quit\n";
            break;
        case INVALID_MESSAGE:
            errorString = "Invalid message received from player\n";
            break;
        case INVALID_REQUEST:
            errorString = "Invalid request by player\n";
            break;
        case SIGINT_ACTION:
            errorString = "SIGINT_ACTION caught\n";
            break;
    }
    fprintf(stderr, "%s", errorString);
    exit((int)exitStatus);
}

/**
* Sets up the hub pipe connection to the player specified. Will exit if piping 
* fails.
*   - players, an array of players containing the desired player
*   - hubPipe, the pipe to be used to send messages from the hub to the player 
*   - playerPipe, the pipe to be used to send messages from the player to the
*     hub
*   - playerNumber, the number of the player to set up pipes to 
*   - game, a struct of the game state
*/
void setup_hub(Player** players, int* hubPipe, int* playerPipe, 
        int playerNumber, Game* game) {
    if (close(hubPipe[PIPE_OUTPUT]) != 0) { 
        exit_program(game, players, PIPING_FAILURE);
    }
    if ((players[playerNumber]->inbox = fdopen(hubPipe[PIPE_INPUT],
            "w")) == NULL) {
        exit_program(game, players, PIPING_FAILURE);
    }
    if (close(playerPipe[PIPE_INPUT]) != 0) {
        exit_program(game, players, PIPING_FAILURE);
    }
    if ((players[playerNumber]->outbox = fdopen(playerPipe[PIPE_OUTPUT],
            "r")) == NULL) {
        exit_program(game, players, PIPING_FAILURE);
    }
    char testCharacter;
    if ((testCharacter = fgetc(players[playerNumber]->outbox)) == EOF) {
        exit_program(game, players, PIPING_FAILURE);
    }
    if (testCharacter != '!') {
        exit_program(game, players, PIPING_FAILURE); 
    }
    players[playerNumber]->status = REMAINING;
}

/**
* Sets up the player pipe connection to the hub. Will exit if piping fails.
*   - players, an array of players containing the desired player
*   - hubPipe, the pipe to be used to send messages from the hub to the player
*   - playerPipe, the pipe to be used to send messages from the player to the
*     hub
*   - playerNumber, the number of the player to set up pipes to
*   - game, a struct of the game state
*/
void setup_player(Player** players, int* hubPipe, int* playerPipe, 
        int playerNumber, Game* game) {
    if (close(hubPipe[PIPE_INPUT]) != 0) {
        exit_program(game, players, PIPING_FAILURE);
    }
    dup2(hubPipe[PIPE_OUTPUT], STDIN_FILENO);
    if (close(playerPipe[PIPE_OUTPUT])) {
        exit_program(game, players, PIPING_FAILURE);
    }
    dup2(playerPipe[PIPE_INPUT], STDOUT_FILENO);
    int devNull = open("/dev/null", O_WRONLY);
    dup2(devNull, STDERR_FILENO);

    char numberPlayers[(int)(MAX_PLAYER_COUNT_STRING_SIZE * sizeof(char))];
    sprintf(numberPlayers, "%d", game->numberOfPlayers); 
    
    char playerTokenString[LABEL_LENGTH + 1];
    playerTokenString[0] = players[playerNumber]->playerToken;
    playerTokenString[LABEL_LENGTH] = '\0';

    execlp(players[playerNumber]->faculty, players[playerNumber]->faculty, 
            numberPlayers, playerTokenString, NULL);
    exit_program(game, players, PIPING_FAILURE);
}

/**
* Sets up the pipe connections from the hub to all players. Will exit if 
* piping fails.
*   - game, a struct of the game state
*   - players, an array of players 
*/
void setup_pipes(Game* game, Player** players) {
    for (int i = 0; i < game->numberOfPlayers; i++) {
        int hubPipe[PIPE_SIZE];
        int playerPipe[PIPE_SIZE];
        if (pipe(hubPipe) != 0) {
            exit_program(game, players, PIPING_FAILURE);
        }
        if (pipe(playerPipe) != 0) {
            exit_program(game, players, PIPING_FAILURE);
        }
        if ((players[i]->pid = fork()) == 0) {
            setup_player(players, hubPipe, playerPipe, i, game);
        } else {
            setup_hub(players, hubPipe, playerPipe, i, game);
        }
    }
}

/**
* Waits for a response from the active player. If the player sends "keepall" 
* returns true, otherwise returns false. If the player sends "reroll", the 
* hub will reroll and send back a response. Will exit if the player has 
* terminated or sends an invalid message.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
bool keep_dice_response(Game* game, Player** players, int activePlayer) {
    char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH];
    char turnReply[MAX_MESSAGE_LENGTH]; 
    if (fgets(turnReply, MAX_MESSAGE_LENGTH, 
            players[activePlayer]->outbox) == NULL) {
        exit_program(game, players, PLAYER_QUIT);
    }
    int numberCommands = interpret_message(turnReply, commands);
    if ((strcmp(commands[0], "keepall") == 0) && numberCommands == 1) {
        return true;
    } else if ((strcmp(commands[0], "reroll") == 0) && numberCommands == 2) {
        if (strlen(commands[REROLLED_DICE_ROLL_INDEX]) > DICE_SET_SIZE ||
                invalid_roll(commands[REROLLED_DICE_ROLL_INDEX])) {
            exit_program(game, players, INVALID_MESSAGE);
        }
        int dicetoReroll = (int)strlen(commands[REROLLED_DICE_ROLL_INDEX]);
        for (int i = 0; i < dicetoReroll; i++) {
            remove_die_from_dice_set(game->rollFile->latestDice, 
                    (char)commands[REROLLED_DICE_ROLL_INDEX][i]);
        }
        add_dice_to_dice_set(game->rollFile, dicetoReroll, 
                game->rollFile->latestDice);
        if (sum_dice_set(game->rollFile->latestDice) != DICE_SET_SIZE) {
            exit_program(game, players, INVALID_REQUEST);
        }
        create_dice_set_string(game->rollFile->latestDice);
        fprintf(players[activePlayer]->inbox, "rerolled %s\n", 
                game->rollFile->latestDice->rollString);
        fflush(players[activePlayer]->inbox);
        return false;
    } else if ((strcmp(commands[0], "stay") == 0) && numberCommands == 1) {
        exit_program(game, players, INVALID_REQUEST);
    } else if ((strcmp(commands[0], "go") == 0) && numberCommands == 1) {
        exit_program(game, players, INVALID_REQUEST);
    } else {
        exit_program(game, players, INVALID_MESSAGE);
    }
    return false;
}

/**
* Generates and saves the final roll (after rerolls) for the active player in
* the game struct. Alerts the players. Will exit if the player has terminated 
* or sends an invalid message.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
void get_player_roll(Game* game, Player** players, int activePlayer) {
    create_dice_set_string(game->rollFile->latestDice);
    fprintf(players[activePlayer]->inbox, "turn %s\n",
            game->rollFile->latestDice->rollString);
    fflush(players[activePlayer]->inbox);

    //continues to send rerolls to player until "keepall" is received
    while (!keep_dice_response(game, players, activePlayer));

    fprintf(stderr, "Player %c rolled %s\n", get_player_label(activePlayer), 
            game->rollFile->latestDice->rollString);
    char rolledAlert[MAX_MESSAGE_LENGTH];
    sprintf(rolledAlert, "rolled %c %s\n",
            get_player_label(activePlayer),
            game->rollFile->latestDice->rollString);
    alert_remaining_players(game, players, activePlayer, rolledAlert, true);
}


/**
* Returns true if the active player is the only player not eliminated. 
* Otherwise returns false.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
bool is_player_last_remaining(Game* game, Player** players,
        int activePlayer) {
    for (int i = 0; i < game->numberOfPlayers; i++) {
        if (i == activePlayer) {
            continue;
        }
        if (players[i]->status != ELIMINATED) {
            return false;
        }
    }
    return true;
}

/**
* Returns true if the active player has reached or exceeded the points needed 
* to win. Otherwise returns false.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
bool has_player_exceeded_win_points(Game* game, Player** players,
        int activePlayer) {
    if (players[activePlayer]->points >= game->scoreLimit) {
        return true;
    } else {
        return false;
    }
}

/**
* Sets the current player as the player in St Lucia. Adds 1 point for taking 
* St Lucia. Alerts the players.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
void claim_stlucia(Game* game, Player** players, int activePlayer) {
    char attackAlert[MAX_MESSAGE_LENGTH];
    game->playerInStLucia = activePlayer;
    sprintf(attackAlert, "claim %c\n", get_player_label(activePlayer));
    fprintf(stderr, "Player %c claimed StLucia\n",
            get_player_label(activePlayer));
    players[activePlayer]->points++; 
    alert_remaining_players(game, players, activePlayer, attackAlert, false);
}

/**
* Waits for a response from the active player after asking "stay?". If the 
* player sends "stay", does nothing. If the player sends "go", the player 
* will claim St Lucia. Will exit if the player in St Lucia has terminated 
* or sends an invalid message.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*   - playerInStLucia, the player who is currently in St Lucia
*/
void receive_stay_reply(Game* game, Player** players, int activePlayer,
        int playerInStLucia) {
    char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH];
    char stayReply[MAX_MESSAGE_LENGTH];
    if (fgets(stayReply, MAX_MESSAGE_LENGTH,
            players[playerInStLucia]->outbox) == NULL) {
        exit_program(game, players, PLAYER_QUIT);
    }
    int numberCommands = interpret_message(stayReply, commands);
    if (players[game->playerInStLucia]->health <= 0) {
        claim_stlucia(game, players, activePlayer);
    } else if ((strcmp(commands[0], "stay") == 0) && numberCommands == 1) {
        //do nothing
    } else if ((strcmp(commands[0], "go") == 0) && numberCommands == 1) {
        claim_stlucia(game, players, activePlayer);
    } else if ((strcmp(commands[0], "keepall") == 0) && numberCommands == 1) {
        exit_program(game, players, INVALID_REQUEST);
    } else if ((strcmp(commands[0], "reroll") == 0) && numberCommands == 2) {
        if (strlen(commands[REROLLED_DICE_ROLL_INDEX]) > DICE_SET_SIZE ||
                invalid_roll(commands[REROLLED_DICE_ROLL_INDEX])) {
            exit_program(game, players, INVALID_REQUEST);
        }
        exit_program(game, players, INVALID_MESSAGE);
    } else {
        exit_program(game, players, INVALID_MESSAGE);
    }
}

/**
* If the active player has rolled any 'A's, they will claim St Lucia if empty.
*  If St Lucia is not empty they will attack inwards if they are not in 
*  St Lucia, or attack outwards if they are. Attacking outwards may result in 
*  claiming St Lucia. Players are alerted of the attacks and their health is 
*  updated. Will exit if the player in St Lucia has terminated or sends an 
*  invalid message.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
void attack(Game* game, Player** players, int activePlayer) {
    char attackAlert[MAX_MESSAGE_LENGTH];
    if (game->rollFile->latestDice->numberOfAs > 0) {
        if (game->playerInStLucia == EMPTY_STLUCIA) {
            claim_stlucia(game, players, activePlayer);
        } else if (game->playerInStLucia == activePlayer) {
            sprintf(attackAlert, "attacks %c %d out\n",
                    get_player_label(activePlayer),
                    game->rollFile->latestDice->numberOfAs);
            for (int i = 0; i < game->numberOfPlayers; i++) {
                if (activePlayer == i ||
                        players[i]->status == ELIMINATED) { 
                    continue;
                }
                damage_player(i, game->rollFile->latestDice->numberOfAs,
                        game, players, true);
            }
            alert_remaining_players(game, players, activePlayer, attackAlert, 
                    false);
        } else {
            sprintf(attackAlert, "attacks %c %d in\n",
                    get_player_label(activePlayer),
                    game->rollFile->latestDice->numberOfAs);
            damage_player(game->playerInStLucia,
                    game->rollFile->latestDice->numberOfAs, game,
                    players, true);
            alert_remaining_players(game, players, activePlayer, attackAlert, 
                    false);
            fprintf(players[game->playerInStLucia]->inbox,
                    "stay?\n");
            fflush(players[game->playerInStLucia]->inbox);
            receive_stay_reply(game, players, activePlayer,
                    game->playerInStLucia);
        }
    }
}

/**
* Increases the active player's points based on their latest dice roll. Alerts 
* the players. 
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*   - startingPoints, the active player's points at the end of their last turn 
*/
void gain_points(Game* game, Player** players, int activePlayer, 
        int startingPoints) {
    players[activePlayer]->tokens +=
            game->rollFile->latestDice->numberOfPs;
    /* Convert tokens to points */
    while (players[activePlayer]->tokens > (TOKENS_POINTS_THRESHOLD - 1)) {
        players[activePlayer]->tokens -= TOKENS_POINTS_THRESHOLD;
        players[activePlayer]->points++;
    }
    if (game->rollFile->latestDice->numberOfOnes > DICE_POINTS_THRESHOLD) {
        players[activePlayer]->points +=
                (game->rollFile->latestDice->numberOfOnes - 
                ONES_DICE_POINT_PENALTY);
    }
    if (game->rollFile->latestDice->numberOfTwos > DICE_POINTS_THRESHOLD) {
        players[activePlayer]->points +=
                (game->rollFile->latestDice->numberOfTwos - 
                TWOS_DICE_POINT_PENALTY);
    }
    if (game->rollFile->latestDice->numberOfThrees > DICE_POINTS_THRESHOLD) {
        players[activePlayer]->points +=
                game->rollFile->latestDice->numberOfThrees - 
                THREES_DICE_POINT_PENALTY;
    }
    int pointsGained = players[activePlayer]->points - startingPoints;
    if (pointsGained > 0) {
        fprintf(stderr, "Player %c scored %d for a total of %d\n",
                get_player_label(activePlayer), pointsGained,
                players[activePlayer]->points);
        char pointsAnnouncement[MAX_MESSAGE_LENGTH];
        sprintf(pointsAnnouncement, "points %c %d\n",
                get_player_label(activePlayer), pointsGained);
        alert_remaining_players(game, players, activePlayer, 
                pointsAnnouncement, false);
    }
}

/**
* Alerts players of newly eliminated players.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
void update_eliminated_players(Game* game, Player** players, 
        int activePlayer) {
    for (int i = 0; i < game->numberOfPlayers; i++) {
        if (players[i]->status == ELIMINATED) {
            continue;
        }
        if (players[i]->health < 1) {
            char eliminatedAnnouncement[MAX_MESSAGE_LENGTH];
            sprintf(eliminatedAnnouncement, "eliminated %c\n",
                    get_player_label(i));
            alert_remaining_players(game, players, activePlayer, 
                    eliminatedAnnouncement, false); 
            players[i]->status = ELIMINATED; 
        }
    }
}

/**
* Returns true if the active player has reached or exceeded the points needed
* to win or there are no other players alive. Alerts players if so. Otherwise
* returns false.
*   - game, a struct of the game state
*   - players, an array of players
*   - activePlayer, the player who is currently having their turn
*/
bool check_game_over(Game* game, Player** players, int activePlayer) {
    char winAnnouncement[MAX_MESSAGE_LENGTH];
    if (is_player_last_remaining(game, players, activePlayer) ||
            has_player_exceeded_win_points(game, players, activePlayer)) {
        fprintf(stderr, "Player %c wins\n",
                get_player_label(activePlayer));
        sprintf(winAnnouncement, "winner %c\n",
                get_player_label(activePlayer));
        alert_remaining_players(game, players, activePlayer, winAnnouncement, 
                false);
        for (int i = 0; i < game->numberOfPlayers; i++) {
            players[i]->status = ELIMINATED;
        }
        return true;
    }
    return false;
}

/**
* Runs the main game loop. Exits if a player quits unexpectedly or sends an
* invalid message.
*   - game, a struct of the game state
*   - players, an array of players
*/
void run_game(Game* game, Player** players) {
    int activePlayer = 0;
    bool winner = false;
    while (!winner) {
        int startingPoints = players[activePlayer]->points;
        if (activePlayer == game->playerInStLucia) {
            players[activePlayer]->points += STARTING_IN_STLUCIA_POINTS;
        }
        reset_dice_set(game->rollFile->latestDice);
        
        add_dice_to_dice_set(game->rollFile, DICE_SET_SIZE,
                game->rollFile->latestDice);
        
        get_player_roll(game, players, activePlayer);
        
        heal(activePlayer, game, players, true,
                game->rollFile->latestDice->numberOfHs);

        attack(game, players, activePlayer);

        gain_points(game, players, activePlayer, startingPoints);

        update_eliminated_players(game, players, activePlayer);

        winner = check_game_over(game, players, activePlayer);

        while (!winner) {
            activePlayer = (activePlayer + 1) % game->numberOfPlayers; 
            if (players[activePlayer]->status != ELIMINATED) {
                break;
            }
        }
    }
}

/**
* Saves the roll file after opening it and checking for errors. Exits if 
* unable to open the roll file or the contents are invalid. 
*   - game, a struct of the game state
*   - filePath, the file path of the roll file 
*/
void create_roll_file(Game* game, char* filePath, Player** players) {
    FILE* loadFile = fopen(filePath, "r");
    /* Array size will dynamically adjust, starting at size 8 */
    int initialArraySize = 8; 
    int index = 0;
    char* rolls = malloc(sizeof(char) * initialArraySize); 
    int currentCharacter;
    if (loadFile == NULL) {
        exit_program(game, players, OPEN_ERROR);
    }
    while (true) {
        currentCharacter = fgetc(loadFile);
        if (feof(loadFile)) {
            if (index == 0) {
                exit_program(game, players, INVALID_FILE);
            }
            break;
        } else if (currentCharacter == '\n') {
            continue;
        } else if (currentCharacter == DICE_CHARACTER_1 || 
                currentCharacter == DICE_CHARACTER_2 ||
                currentCharacter == DICE_CHARACTER_3 || 
                currentCharacter == DICE_CHARACTER_4 ||
                currentCharacter == DICE_CHARACTER_5 || 
                currentCharacter == DICE_CHARACTER_6) {
            rolls[index] = currentCharacter;
            index++;
            if (index > initialArraySize / 2) {
                initialArraySize *= 2;
                rolls = (char*)realloc(rolls, initialArraySize);
            }
            continue;
        }
        exit_program(game, players, INVALID_FILE);
    }
    game->rollFile->index = 0;
    game->rollFile->size = index;
    game->rollFile->diceRolls = malloc(sizeof(char) * initialArraySize);
    for (int i = 0; i < index; i++) {
        game->rollFile->diceRolls[i] = rolls[i];
    }
    free(rolls);
}

/**
* Executes when a SIGINT occurs. Exits program and shuts down players. 
*/
void catch_sigint() {
    exit_program(game, players, SIGINT_ACTION);
}


int main(int argc, char** argv) {
    //Define SIGINT and SIGPIPE handlers
    struct sigaction sigint;
    sigint.sa_handler = catch_sigint;
    sigint.sa_flags = SA_RESTART; 
    sigaction(SIGINT, &sigint, 0);

    struct sigaction sigpipe;
    sigpipe.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigpipe, 0);

    game = malloc(sizeof(Game));
    initialise_game(game);
    game->mallocProgress = GAME;

    if (argc < HUB_MIN_ARGS || argc > HUB_MAX_ARGS) {
        exit_program(game, NULL, INVALID_ARGUMENTS);
    }
    game->scoreLimit = atoi(argv[HUB_SCORE_LIMIT_ARGUMENT_INDEX]);
    if (game->scoreLimit <= 0) {
        exit_program(game, NULL, INVALID_SCORE);
    }
    
    game->numberOfPlayers = argc - HUB_ARGUMENTS_NOT_PLAYERS;

    players = malloc(sizeof(Player*) * game->numberOfPlayers);
    initialise_players(game, players);
    game->mallocProgress = PLAYERS;
    for (int i = 0; i < game->numberOfPlayers; i++) {
        players[i]->faculty = argv[i + HUB_ARGUMENTS_NOT_PLAYERS];
        players[i]->playerToken = get_player_label(i);
    }

    create_roll_file(game, argv[1], players);
    game->mallocProgress = ROLL_FILE;

    setup_pipes(game, players);

    run_game(game, players);

    exit_program(game, players, SUCCESS);
    return 0; 
}
