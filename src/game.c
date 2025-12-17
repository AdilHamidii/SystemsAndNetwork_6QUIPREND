#include "headers/game.h"

//Cette fonction retourne le nombre de têtes de bœufs d’une carte c.
int bulls(int c) {
 if (c == 55) return 7; // Carte spéciale 55 qui 7 têtes de bœufs
 if (c % 11 == 0) return 5; // Multiple de 11 qui 5 points
 if (c % 10 == 0) return 3; 
 if (c % 5 == 0) return 2; 
 return 1; 
}

// Échange de deux entiers (utilisé pour le mélange et le tri)
static void swap_int(int *a, int *b) {
 int t = *a;
 *a = *b;
 *b = t;
}


// Mélange aléatoire du paquet de cartes
void game_shuffle(Game *g) {
 // Initialisation du générateur pseudo-aléatoire
 // time(NULL) + PID pour éviter les répétitions
 srand((unsigned)time(NULL) ^ (unsigned)getpid());

 // Algorithme de Fisher-Yates
 for (int i = DECK_SIZE - 1; i > 0; i--) {
 int j = rand() % (i + 1);
 swap_int(&g->deck[i], &g->deck[j]);
 }
}


// Initialisation complète d’une partie
void game_init(Game *g, int nplayers) {
 memset(g, 0, sizeof(*g)); // Remise à zéro de toute la structure Game
 g->nplayers = nplayers; // Nombre de joueurs

 // Initialisation du deck avec les cartes 1 à 104
 for (int i = 0; i < DECK_SIZE; i++) g->deck[i] = i + 1;

 g->top = 0; // Indice du sommet du paquet

 // Initialisation des rangées
 for (int r = 0; r < ROWS; r++) g->rows[r].len = 0;

 // Initialisation des joueurs
 for (int p = 0; p < MAX_PLAYERS; p++) {
 g->scores[p] = 0; // Score initial à 0
 g->hand_len[p] = 0; // Main vide
 g->carte_jouee[p] = -1;// Aucune carte jouée
 }

 g->manche = 1; // Première manche
 g->tour = 1; // Premier tour
 g->fin = 0; // Partie non terminée

 game_shuffle(g); // Mélange du paquet
}

// Mise en place des 4 rangées au début d’une manche
void game_setup_rows(Game *g) {
 for (int r = 0; r < ROWS; r++) {
 g->rows[r].len = 1; // Une carte par rangée
 g->rows[r].cards[0] = g->deck[g->top++]; // Carte tirée du paquet
 }
}

// Distribution des cartes aux joueurs
void game_deal(Game *g) {
 for (int p = 0; p < g->nplayers; p++) {
 g->hand_len[p] = HAND_SIZE; // Chaque joueur reçoit HAND_SIZE cartes

 // Distribution des cartes
 for (int k = 0; k < HAND_SIZE; k++) {
 g->hands[p][k] = g->deck[g->top++];
 }

 // Tri de la main du joueur (ordre croissant)
 for (int i = 0; i < HAND_SIZE; i++) {
 for (int j = i + 1; j < HAND_SIZE; j++) {
 if (g->hands[p][j] < g->hands[p][i])
 swap_int(&g->hands[p][i], &g->hands[p][j]);
 }
 }
 }
}
// Vérifie si une carte c est présente dans la main du joueur pid
int game_hand_has(Game *g, int pid, int c) {
 for (int i = 0; i < g->hand_len[pid]; i++)
 if (g->hands[pid][i] == c) return 1;
 return 0;
}

// Retire une carte c de la main du joueur pid
int game_hand_remove(Game *g, int pid, int c) {
 int n = g->hand_len[pid];
 for (int i = 0; i < n; i++) {
 if (g->hands[pid][i] == c) {
 // Décalage des cartes vers la gauche
 for (int j = i + 1; j < n; j++)
 g->hands[pid][j-1] = g->hands[pid][j];

 g->hand_len[pid]--; // Réduction de la taille de la main
 return 1;
 }
 }
 return 0;
}

// Retourne la dernière carte d’une rangée
static int row_last(Row *r) {
 return r->cards[r->len - 1];
}

// Calcule le total de têtes de bœufs d’une rangée
static int row_bulls(Row *r) {
 int s = 0;
 for (int i = 0; i < r->len; i++)
 s += bulls(r->cards[i]);
 return s;
}

// Trouve la meilleure rangée où placer la carte c
static int best_row_for_card(Game *g, int c, int *out_diff) {
 int best = -1;
 int bestdiff = 0x7fffffff; // Valeur très grande

 for (int r = 0; r < ROWS; r++) {
 int last = row_last(&g->rows[r]);

 // La carte doit être plus grande que la dernière de la rangée
 if (c > last) {
 int d = c - last;
 if (d < bestdiff) {
 bestdiff = d;
 best = r;
 }
 }
 }
 if (best >= 0 && out_diff) *out_diff = bestdiff;
 return best; // -1 si aucune rangée possible
}

// Retourne la rangée ayant le moins de têtes de bœufs
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

// Placement d’une carte pour un joueur
int game_place_card(Game *g, int pid, int c,
 int chosen_row_if_needed,
 int *out_row_taken,
 int *out_bulls_taken) {

 int taken_row = -1; // Rangée ramassée
 int bulls_taken = 0; // Points gagnés

 int r = best_row_for_card(g, c, NULL);
 // Cas où la carte est plus petite que toutes les rangées
 if (r < 0) {
 int cr = chosen_row_if_needed;
 if (cr < 0 || cr >= ROWS) cr = min_bulls_row(g);

 taken_row = cr;
 bulls_taken = row_bulls(&g->rows[cr]);
 g->scores[pid] += bulls_taken;

 // Réinitialisation de la rangée
 g->rows[cr].len = 1;
 g->rows[cr].cards[0] = c;
 } else {
 // Cas où la rangée est pleine (6ème carte)
 if (g->rows[r].len == ROW_MAX) {
 taken_row = r;
 bulls_taken = row_bulls(&g->rows[r]);
 g->scores[pid] += bulls_taken;

 g->rows[r].len = 1;
 g->rows[r].cards[0] = c;
 } else {
 // Ajout normal de la carte
 g->rows[r].cards[g->rows[r].len++] = c;
 }
 }

 // Valeurs de sortie
 if (out_row_taken) *out_row_taken = taken_row;
 if (out_bulls_taken) *out_bulls_taken = bulls_taken;

 return 1;
}



// Vérifie si la partie est terminée
int game_over(Game *g, int limit) {
 for (int p = 0; p < g->nplayers; p++)
 if (g->scores[p] >= limit) return 1;

 if (g->fin) return 1;
 return 0;
}

// Génère une chaîne représentant l’état de la table
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

// Génère une chaîne représentant la main d’un joueur
void game_hand_string(Game *g, int pid, char *buf, int cap) {
 char tmp[64];
 buf[0] = 0;

 for (int i = 0; i < g->hand_len[pid]; i++) {
 snprintf(tmp, sizeof(tmp), "%d", g->hands[pid][i]);
 strncat(buf, tmp, cap - (int)strlen(buf) - 1);
 if (i + 1 < g->hand_len[pid])
 strncat(buf, " ", cap - (int)strlen(buf) - 1);
 }
}

// Génère une chaîne représentant les scores
void game_score_string(Game *g, char *buf, int cap) {
 char tmp[64];
 buf[0] = 0;

 for (int p = 0; p < g->nplayers; p++) {
 snprintf(tmp, sizeof(tmp), "J%d=%d", p+1, g->scores[p]);
 strncat(buf, tmp, cap - (int)strlen(buf) - 1);
 if (p + 1 < g->nplayers)
 strncat(buf, " ", cap - (int)strlen(buf) - 1);
 }
}