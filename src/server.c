#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"
#include "headers/game.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

typedef struct {
    int fd;
    FILE *in;
    FILE *out;
    int connected;
    char name[PLAYER_NAME_MAX];

    int played;
    int card;
    int chosen_row;
} Player;

static void sendf(FILE *out, const char *fmt, ...) {
    char buf[LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    send_line(out, buf);
}

static void broadcast(Player *p, int n, const char *msg) {
    for (int i = 0; i < n; i++)
        if (p[i].connected)
            send_line(p[i].out, msg);
}

static int all_played(Player *p, int n) {
    for (int i = 0; i < n; i++)
        if (p[i].connected && !p[i].played)
            return 0;
    return 1;
}

static int parse_int_strict(const char *s, int *out) {
    char *end = NULL;
    long v;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return 0;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0) return 0;
    while (end && *end && isspace((unsigned char)*end)) end++;
    if (end && *end != 0) return 0;
    if (v < -2147483648L || v > 2147483647L) return 0;
    *out = (int)v;
    return 1;
}

static int str_ieq_n(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (cb == 0) return 1;
        if (ca == 0) return 0;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return 1;
}

static int parse_play_cmd(const char *line, int *out_card) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (*line == 0) return 0;

    if (isdigit((unsigned char)*line) || *line == '+' || *line == '-')
        return parse_int_strict(line, out_card);

    if (str_ieq_n(line, "JOUER", 5)) {
        line += 5;
        while (*line && isspace((unsigned char)*line)) line++;
        return parse_int_strict(line, out_card);
    }

    return 0;
}

static int parse_row_cmd(const char *line, int *out_row0) {
    int r;
    while (*line && isspace((unsigned char)*line)) line++;
    if (*line == 0) return 0;

    if (isdigit((unsigned char)*line) || *line == '+' || *line == '-') {
        if (!parse_int_strict(line, &r)) return 0;
        r -= 1;
        if (r < 0 || r >= ROWS) return 0;
        *out_row0 = r;
        return 1;
    }

    if (str_ieq_n(line, "RANGE", 5)) {
        line += 5;
        while (*line && isspace((unsigned char)*line)) line++;
        if (!parse_int_strict(line, &r)) return 0;
        r -= 1;
        if (r < 0 || r >= ROWS) return 0;
        *out_row0 = r;
        return 1;
    }

    if (str_ieq_n(line, "RANGEE", 6)) {
        line += 6;
        while (*line && isspace((unsigned char)*line)) line++;
        if (!parse_int_strict(line, &r)) return 0;
        r -= 1;
        if (r < 0 || r >= ROWS) return 0;
        *out_row0 = r;
        return 1;
    }

    if (str_ieq_n(line, "PRENDRE", 7)) {
        line += 7;
        while (*line && isspace((unsigned char)*line)) line++;
        if (!parse_int_strict(line, &r)) return 0;
        r -= 1;
        if (r < 0 || r >= ROWS) return 0;
        *out_row0 = r;
        return 1;
    }

    return 0;
}

static int row_last(const Game *g, int r) {
    return g->rows[r].cards[g->rows[r].len - 1];
}

static int needs_row_choice(const Game *g, int c) {
    for (int r = 0; r < ROWS; r++)
        if (c > row_last(g, r))
            return 0;
    return 1;
}

static void ask_row_until_valid(Game *g, Player *pl, int card) {
    char line[LINE_MAX];
    for (;;) {
        char table[LINE_MAX];
        game_table_string(g, table, sizeof(table));
        sendf(pl->out, "INFO Carte %d: choisissez la rangee a ramasser (1-%d).", card, ROWS);
        send_line(pl->out, table);
        send_line(pl->out, "CHOISIR_RANGEES");

        if (!recv_line(pl->in, line, sizeof(line)))
            die("Deconnexion joueur");

        int r0;
        if (!parse_row_cmd(line, &r0)) {
            sendf(pl->out, "ERREUR Choix invalide. Entrez un numero entre 1 et %d.", ROWS);
            continue;
        }
        pl->chosen_row = r0;
        return;
    }
}

static void reorder_by_card(const Game *g, int order[MAX_PLAYERS]) {
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
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <nb_joueurs>\n", argv[0]);
        return 1;
    }

    int nb = atoi(argv[2]);
    if (nb < 2 || nb > MAX_PLAYERS)
        die("Nombre de joueurs invalide");

    int listen_fd = tcp_listen(argv[1]);
    if (listen_fd < 0)
        die("listen");

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    for (int i = 0; i < MAX_PLAYERS; i++) players[i].fd = -1;

    printf("Serveur: ecoute sur le port %s, joueurs=%d\n", argv[1], nb);

    for (int i = 0; i < nb; i++) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);

        int fd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (fd < 0) die("accept");

        players[i].fd = fd;
        players[i].in = fdopen_r(fd);
        players[i].out = fdopen_w(fd);
        players[i].connected = 1;

        char line[LINE_MAX];
        if (!recv_line(players[i].in, line, sizeof(line)))
            die("Deconnexion pendant NOM");

        strncpy(players[i].name, line, PLAYER_NAME_MAX - 1);
        players[i].name[PLAYER_NAME_MAX - 1] = 0;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        printf("Connexion: J%d (%s) depuis %s\n", i + 1, players[i].name, ip);
    }

    broadcast(players, nb, "INFO Tous les joueurs sont connectes.");

    Game game;
    game_init(&game, nb);
    game_setup_rows(&game);
    game_deal(&game);

    while (!game_over(&game, 66)) {
        char table[LINE_MAX];
        game_table_string(&game, table, sizeof(table));
        printf("\nTABLE: %s\n", table);
        broadcast(players, nb, table);

        for (int i = 0; i < nb; i++) {
            char hand[LINE_MAX];
            game_hand_string(&game, i, hand, sizeof(hand));
            sendf(players[i].out, "MAIN %s", hand);
            players[i].played = 0;
            players[i].card = -1;
            players[i].chosen_row = -1;
        }

        broadcast(players, nb, "DEMANDE_CARTE");

        while (!all_played(players, nb)) {
            fd_set rfds;
            FD_ZERO(&rfds);
            int maxfd = -1;

            for (int i = 0; i < nb; i++) {
                if (players[i].connected && !players[i].played && players[i].fd >= 0) {
                    FD_SET(players[i].fd, &rfds);
                    if (players[i].fd > maxfd) maxfd = players[i].fd;
                }
            }

            if (maxfd < 0) die("Plus de joueurs");

            if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
                die("select");

            for (int i = 0; i < nb; i++) {
                if (players[i].connected && !players[i].played && players[i].fd >= 0 &&
                    FD_ISSET(players[i].fd, &rfds)) {

                    char line[LINE_MAX];
                    if (!recv_line(players[i].in, line, sizeof(line)))
                        die("Deconnexion joueur");

                    int c;
                    if (!parse_play_cmd(line, &c)) {
                        send_line(players[i].out, "ERREUR Commande invalide. Exemple: JOUER 42 (ou juste 42).");
                        send_line(players[i].out, "DEMANDE_CARTE");
                        continue;
                    }

                    if (!game_hand_has(&game, i, c)) {
                        sendf(players[i].out, "ERREUR Carte %d absente de votre main.", c);
                        send_line(players[i].out, "DEMANDE_CARTE");
                        continue;
                    }

                    if (!game_hand_remove(&game, i, c)) {
                        sendf(players[i].out, "ERREUR Carte %d impossible a retirer (etat interne).", c);
                        send_line(players[i].out, "DEMANDE_CARTE");
                        continue;
                    }

                    game.carte_jouee[i] = c;
                    players[i].card = c;
                    players[i].played = 1;

                    printf("J%d (%s) joue %d\n", i + 1, players[i].name, c);
                }
            }
        }

        int order[MAX_PLAYERS];
        reorder_by_card(&game, order);

        int taken_row[MAX_PLAYERS];
        int bulls_taken[MAX_PLAYERS];
        for (int i = 0; i < nb; i++) {
            taken_row[i] = -1;
            bulls_taken[i] = 0;
        }

        for (int k = 0; k < nb; k++) {
            int pid = order[k];
            int c = game.carte_jouee[pid];

            if (needs_row_choice(&game, c)) {
                ask_row_until_valid(&game, &players[pid], c);
            } else {
                players[pid].chosen_row = -1;
            }

            int tr = -1, bt = 0;
            game_place_card(&game, pid, c, players[pid].chosen_row, &tr, &bt);
            taken_row[pid] = tr;
            bulls_taken[pid] = bt;
        }

        for (int p = 0; p < nb; p++) game.carte_jouee[p] = -1;

        for (int i = 0; i < nb; i++) {
            if (taken_row[i] >= 0) {
                printf("J%d (%s) ramasse R%d (+%d)\n",
                       i + 1, players[i].name, taken_row[i] + 1, bulls_taken[i]);
            }
        }

        char score[LINE_MAX];
        game_score_string(&game, score, sizeof(score));
        printf("SCORES: %s\n", score);
        broadcast(players, nb, score);

        game.tour++;
        if (game.tour > HAND_SIZE) {
            game.manche++;
            game.tour = 1;
            if (game.top + ROWS + game.nplayers * HAND_SIZE > DECK_SIZE) {
                game.fin = 1;
            } else {
                game_setup_rows(&game);
                game_deal(&game);
            }
        }
    }

    printf("\nFIN DE PARTIE\n");

    for (int i = 0; i < nb; i++) {
        fclose(players[i].in);
        fclose(players[i].out);
        close(players[i].fd);
    }

    close(listen_fd);
    return 0;
}
