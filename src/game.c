#include "headers/game.h"

int bulls(int c) {
    if (c == 55) return 7;
    if (c % 11 == 0) return 5;
    if (c % 10 == 0) return 3;
    if (c % 5 == 0) return 2;
    return 1;
}

static void swap_int(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

void game_shuffle(Game *g) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (int i = DECK_SIZE - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        swap_int(&g->deck[i], &g->deck[j]);
    }
}

void game_init(Game *g, int nplayers) {
    memset(g, 0, sizeof(*g));
    g->nplayers = nplayers;
    for (int i = 0; i < DECK_SIZE; i++) g->deck[i] = i + 1;
    g->top = 0;
    for (int r = 0; r < ROWS; r++) g->rows[r].len = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        g->scores[p] = 0;
        g->hand_len[p] = 0;
        g->carte_jouee[p] = -1;
    }
    g->manche = 1;
    g->tour = 1;
    g->fin = 0;
    game_shuffle(g);
}

void game_setup_rows(Game *g) {
    for (int r = 0; r < ROWS; r++) {
        g->rows[r].len = 1;
        g->rows[r].cards[0] = g->deck[g->top++];
    }
}

void game_deal(Game *g) {
    for (int p = 0; p < g->nplayers; p++) {
        g->hand_len[p] = HAND_SIZE;
        for (int k = 0; k < HAND_SIZE; k++) {
            g->hands[p][k] = g->deck[g->top++];
        }
        for (int i = 0; i < HAND_SIZE; i++) {
            for (int j = i + 1; j < HAND_SIZE; j++) {
                if (g->hands[p][j] < g->hands[p][i]) swap_int(&g->hands[p][i], &g->hands[p][j]);
            }
        }
    }
}

int game_hand_has(Game *g, int pid, int c) {
    for (int i = 0; i < g->hand_len[pid]; i++) if (g->hands[pid][i] == c) return 1;
    return 0;
}

int game_hand_remove(Game *g, int pid, int c) {
    int n = g->hand_len[pid];
    for (int i = 0; i < n; i++) {
        if (g->hands[pid][i] == c) {
            for (int j = i + 1; j < n; j++) g->hands[pid][j-1] = g->hands[pid][j];
            g->hand_len[pid]--;
            return 1;
        }
    }
    return 0;
}

static int row_last(Row *r) {
    return r->cards[r->len - 1];
}

static int row_bulls(Row *r) {
    int s = 0;
    for (int i = 0; i < r->len; i++) s += bulls(r->cards[i]);
    return s;
}

static int best_row_for_card(Game *g, int c, int *out_diff) {
    int best = -1;
    int bestdiff = 0x7fffffff;
    for (int r = 0; r < ROWS; r++) {
        int last = row_last(&g->rows[r]);
        if (c > last) {
            int d = c - last;
            if (d < bestdiff) {
                bestdiff = d;
                best = r;
            }
        }
    }
    if (best >= 0 && out_diff) *out_diff = bestdiff;
    return best;
}

static int min_bulls_row(Game *g) {
    int best = 0;
    int bestv = row_bulls(&g->rows[0]);
    for (int r = 1; r < ROWS; r++) {
        int v = row_bulls(&g->rows[r]);
        if (v < bestv) {
            bestv = v;
            best = r;
        }
    }
    return best;
}

int game_place_card(Game *g, int pid, int c, int chosen_row_if_needed, int *out_row_taken, int *out_bulls_taken) {
    int taken_row = -1;
    int bulls_taken = 0;

    int r = best_row_for_card(g, c, NULL);
    if (r < 0) {
        int cr = chosen_row_if_needed;
        if (cr < 0 || cr >= ROWS) cr = min_bulls_row(g);
        taken_row = cr;
        bulls_taken = row_bulls(&g->rows[cr]);
        g->scores[pid] += bulls_taken;
        g->rows[cr].len = 1;
        g->rows[cr].cards[0] = c;
    } else {
        if (g->rows[r].len == ROW_MAX) {
            taken_row = r;
            bulls_taken = row_bulls(&g->rows[r]);
            g->scores[pid] += bulls_taken;
            g->rows[r].len = 1;
            g->rows[r].cards[0] = c;
        } else {
            g->rows[r].cards[g->rows[r].len++] = c;
        }
    }

    if (out_row_taken) *out_row_taken = taken_row;
    if (out_bulls_taken) *out_bulls_taken = bulls_taken;
    return 1;
}

void game_apply_turn(Game *g, int chosen_row[MAX_PLAYERS], int out_taken_row[MAX_PLAYERS], int out_bulls[MAX_PLAYERS]) {
    int order[MAX_PLAYERS];
    for (int i = 0; i < g->nplayers; i++) order[i] = i;

    for (int i = 0; i < g->nplayers; i++) {
        for (int j = i + 1; j < g->nplayers; j++) {
            int pi = order[i], pj = order[j];
            if (g->carte_jouee[pj] < g->carte_jouee[pi]) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    for (int i = 0; i < g->nplayers; i++) {
        int pid = order[i];
        int c = g->carte_jouee[pid];
        int tr = -1, bt = 0;
        game_place_card(g, pid, c, chosen_row[pid], &tr, &bt);
        out_taken_row[pid] = tr;
        out_bulls[pid] = bt;
    }

    for (int p = 0; p < g->nplayers; p++) g->carte_jouee[p] = -1;

    g->tour++;
    if (g->tour > HAND_SIZE) {
        g->manche++;
        g->tour = 1;
        if (g->top + ROWS + g->nplayers * HAND_SIZE > DECK_SIZE) {
            g->fin = 1;
        } else {
            game_setup_rows(g);
            game_deal(g);
        }
    }
}

int game_over(Game *g, int limit) {
    for (int p = 0; p < g->nplayers; p++) if (g->scores[p] >= limit) return 1;
    if (g->fin) return 1;
    return 0;
}

void game_table_string(Game *g, char *buf, int cap) {
    char tmp[256];
    buf[0] = 0;
    for (int r = 0; r < ROWS; r++) {
        snprintf(tmp, sizeof(tmp), "R%d:", r+1);
        strncat(buf, tmp, cap - (int)strlen(buf) - 1);
        for (int i = 0; i < g->rows[r].len; i++) {
            snprintf(tmp, sizeof(tmp), " %d", g->rows[r].cards[i]);
            strncat(buf, tmp, cap - (int)strlen(buf) - 1);
        }
        strncat(buf, " | ", cap - (int)strlen(buf) - 1);
    }
}

void game_hand_string(Game *g, int pid, char *buf, int cap) {
    char tmp[64];
    buf[0] = 0;
    for (int i = 0; i < g->hand_len[pid]; i++) {
        snprintf(tmp, sizeof(tmp), "%d", g->hands[pid][i]);
        strncat(buf, tmp, cap - (int)strlen(buf) - 1);
        if (i + 1 < g->hand_len[pid]) strncat(buf, " ", cap - (int)strlen(buf) - 1);
    }
}

void game_score_string(Game *g, char *buf, int cap) {
    char tmp[64];
    buf[0] = 0;
    for (int p = 0; p < g->nplayers; p++) {
        snprintf(tmp, sizeof(tmp), "J%d=%d", p+1, g->scores[p]);
        strncat(buf, tmp, cap - (int)strlen(buf) - 1);
        if (p + 1 < g->nplayers) strncat(buf, " ", cap - (int)strlen(buf) - 1);
    }
}
