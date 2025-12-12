#ifndef GAME_H
#define GAME_H

#include "common.h"

typedef struct {
    int cards[ROW_MAX];
    int len;
} Row;

typedef struct {
    int deck[DECK_SIZE];
    int top;

    Row rows[ROWS];

    int scores[MAX_PLAYERS];
    int hands[MAX_PLAYERS][HAND_SIZE];
    int hand_len[MAX_PLAYERS];

    int nplayers;
    int manche;
    int tour;
    int fin;

    int carte_jouee[MAX_PLAYERS];
} Game;

int bulls(int c);

void game_init(Game *g, int nplayers);
void game_shuffle(Game *g);
void game_setup_rows(Game *g);
void game_deal(Game *g);

int game_hand_has(Game *g, int pid, int c);
int game_hand_remove(Game *g, int pid, int c);

int game_place_card(Game *g, int pid, int c, int chosen_row_if_needed, int *out_row_taken, int *out_bulls_taken);

void game_apply_turn(Game *g, int chosen_row[MAX_PLAYERS], int out_taken_row[MAX_PLAYERS], int out_bulls[MAX_PLAYERS]);

int game_over(Game *g, int limit);

void game_table_string(Game *g, char *buf, int cap);
void game_hand_string(Game *g, int pid, char *buf, int cap);
void game_score_string(Game *g, char *buf, int cap);

#endif
