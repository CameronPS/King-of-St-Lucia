#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef SHARED_H
#define SHARED_H

/* Game properties */
#define MIN_PLAYERS 2
#define MAX_PLAYERS 26
#define DICE_SET_SIZE 6
#define FIRST_PLAYER_LETTER 'A'
#define LABEL_LENGTH 1
#define STARTING_HEALTH 10
#define EMPTY_STLUCIA -1

/* Messages sent to the player */
#define MAX_MESSAGE_LENGTH 40
#define MAX_COMMANDS 5

/* The valid characters that comprise the dice rolls */
#define DICE_CHARACTER_1 '1'
#define DICE_CHARACTER_2 '2'
#define DICE_CHARACTER_3 '3'
#define DICE_CHARACTER_4 'H'
#define DICE_CHARACTER_5 'A'
#define DICE_CHARACTER_6 'P'

/**
* A struct for representing dice rolls internally. Six dice rolls is referred
* to as a dice 'set'.
*   - numberOfOnes, the number of '1's rolled
*   - numberOfTwos, the number of '2's rolled
*   - numberOfThrees, the number of '3's rolled
*   - numberOfHs, the number of 'H's rolled
*   - numberOfAs, the number of 'A's rolled
*   - numberOfPs, the number of 'P's rolled
*/
typedef struct {
    int numberOfOnes;
    int numberOfTwos;
    int numberOfThrees;
    int numberOfHs;
    int numberOfAs;
    int numberOfPs;
    char* rollString;
} DiceSet;

/**
* A struct for storing the variables relating to the roll file, along with 
* three dice sets.
*   - size, the number of rolls in the roll file
*   - index, the index of the next dice to be rolled 
*   - diceRolls, an array containing the dice rolls
*   - latestDice, a DiceSet of the latest dice rolled 
*   - rerollDice, a DiceSet of the dice being rerolled
*   - oppositionDice, a DiceSet for the oppositions dice rolls
*/
typedef struct {
    int size;
    int index;
    char* diceRolls;
    DiceSet* latestDice;
    DiceSet* rerollDice;
    DiceSet* oppositionDice;
} RollFile;

/**
* An enum struct for storing the state of the player.
*   - ELIMINATED, the player has been eliminated
*   - REMAINING, the player has not been eliminated
*   - UNCONNECTED, the player has not been piped to yet
*/
typedef enum {
    ELIMINATED,
    REMAINING,
    UNCONNECTED,
} PlayerStatus;

/**
* An enum for recording malloc progress
*   - NONE, no memory has been malloc'd
*   - GAME, game has been malloc'd
*   - PLAYERS, players have been malloc'd
*   - ROLLFILE, roll file has been malloc'd
*/
typedef enum {
    NONE = 0,
    GAME = 1,
    PLAYERS = 2,
    ROLL_FILE = 3,
} MallocProgress;

/**
* A struct for storing player information.
*   - inbox, the file stream of incoming messages
*   - outbox, the file stream for outgoing messages
*   - pid, the pid of the player
*   - playerToken, the player token eg 'A'
*   - faculty,  the player faculty eg "./EAIT"
*   - health, the player health
*   - points, the player points
*   - tokens, the player token count
*   - status, the player status
*/
typedef struct {
    FILE* inbox;
    FILE* outbox;
    pid_t pid;
    char playerToken;
    char* faculty;
    int health;
    int points;
    int tokens;
    PlayerStatus status;
} Player;

/**
* A struct for storing game information.
*   - scoreLimit, the score limit for the game
*   - playerInStLucia, the number of the player in St Lucia 
*   - numberOfPlayers, the number of players in the game 
*   - currentPlayerNumber,  the current player number 
*   - rollFile, a link to the roll file
*   - numberOfRerolls, the number of times the player has rerolled this 
*       turn
*   -mallocProgress, represents which memory has been allocated with malloc
*   */
typedef struct {
    int scoreLimit;
    int playerInStLucia;
    int numberOfPlayers;
    int currentPlayerNumber;
    RollFile* rollFile;
    int numberOfRerolls;
    MallocProgress mallocProgress;
} Game;

/* Function prototypes */
void initialise_game(Game* game);
void initialise_players(Game*, Player** players);
void intialise_roll_file(RollFile* rollFile);
void reset_dice_set(DiceSet* latestDice);
int get_player_number(char playerToken);
char get_player_label(int playerNumber);
void update_dice_set(DiceSet* latestDice, char die, int update);
void add_die_to_dice_set(DiceSet* latestDice, char die);
void remove_die_from_dice_set(DiceSet* latestDice, char die);
void print_dice_set(DiceSet* latestDice);
void add_dice_to_dice_set(RollFile* rollFile, int numberOfDice, 
        DiceSet* latestDice);
void add_die_type(char* diceSet, int numberOfDice, char valueToAdd, 
        int* index);
void create_dice_set_string(DiceSet* latestDice);
int sum_dice_set(DiceSet* latestDice);
int interpret_message(char* message, 
        char commands[MAX_COMMANDS][MAX_MESSAGE_LENGTH]);
void heal(int activePlayer, Game* game, Player** players, bool isHub, 
        int healing);
void damage_player(int player, int damage, Game* game, Player** players, 
        bool isHub);
void reroll(Game* game, Player** players, RollFile* rollFile);
void act_on_dice(Game* game, Player** players);
bool retreat(Game* game, Player** players);
int players_remaining(Game* game, Player** players);
bool invalid_roll(char* message);
void free_allocated_memory(Game* game, Player** players);
#endif

