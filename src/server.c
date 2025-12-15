#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"
#include "headers/game.h"
#include <stdarg.h>

typedef struct {
    int fd;
    FILE *in;
    FILE *out;
    int connected;
    char name[PLAYER_NAME_MAX];

    int played;
    int card;
    int needs_row;
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
        if (!p[i].played)
            return 0;
    return 1;
}

static int any_need_row(Player *p, int n) {
    for (int i = 0; i < n; i++)
        if (p[i].needs_row)
            return 1;
    return 0;
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <nb_joueurs>\n", argv[0]);
        return 1;
    }

    int port_players = atoi(argv[2]);
    if (port_players < 2 || port_players > MAX_PLAYERS)
        die("Nombre de joueurs invalide");

    int listen_fd = tcp_listen(argv[1]);
    if (listen_fd < 0)
        die("listen");

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    for (int i = 0; i < MAX_PLAYERS; i++)
        players[i].fd = -1;

    printf("Serveur: ecoute sur le port %s, joueurs=%d\n",
           argv[1], port_players);

    /* ---------- accept players ---------- */

    for (int i = 0; i < port_players; i++) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);

        int fd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (fd < 0)
            die("accept");

        players[i].fd = fd;
        players[i].in = fdopen_r(fd);
        players[i].out = fdopen_w(fd);
        players[i].connected = 1;

        char line[LINE_MAX];
        recv_line(players[i].in, line, sizeof(line)); /* NOM déjà envoyé */
        strncpy(players[i].name, line, PLAYER_NAME_MAX - 1);

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));

        printf("Connexion: J%d (%s) depuis %s\n",
               i + 1, players[i].name, ip);
    }

    broadcast(players, port_players,
              "INFO Tous les joueurs sont connectes.");

    /* ---------- game init ---------- */

    Game game;
    game_init(&game, port_players);
    game_setup_rows(&game);
    game_deal(&game);

    /* ---------- game loop ---------- */

    while (!game_over(&game, 66)) {
        char buf[LINE_MAX];

        game_table_string(&game, buf, sizeof(buf));
        printf("\nTABLE: %s\n", buf);
        broadcast(players, port_players, buf);

        for (int i = 0; i < port_players; i++) {
            char hand[LINE_MAX];
            game_hand_string(&game, i, hand, sizeof(hand));
            sendf(players[i].out, "MAIN %s", hand);

            players[i].played = 0;
            players[i].needs_row = 0;
            players[i].chosen_row = -1;
        }

        broadcast(players, port_players, "DEMANDE_CARTE");

        /* ----- wait cards ----- */
        while (!all_played(players, port_players)) {
            fd_set rfds;
            FD_ZERO(&rfds);
            int maxfd = -1;

            for (int i = 0; i < port_players; i++) {
                if (players[i].fd >= 0) {
                    FD_SET(players[i].fd, &rfds);
                    if (players[i].fd > maxfd)
                        maxfd = players[i].fd;
                }
            }

            select(maxfd + 1, &rfds, NULL, NULL, NULL);

            for (int i = 0; i < port_players; i++) {
                if (players[i].fd >= 0 &&
                    FD_ISSET(players[i].fd, &rfds)) {

                    char line[LINE_MAX];
                    recv_line(players[i].in, line, sizeof(line));
                       
                    if (strncmp(line, "JOUER ", 6) == 0) {
                        int c = atoi(line + 6);
                        game_hand_remove(&game, i, c);
                        game.carte_jouee[i] = c;

                        players[i].played = 1;

                        printf("J%d (%s) joue %d\n",
                               i + 1, players[i].name, c);
                    }
                    else printf("J%d (%s) envoi commande inconnue: %s\n",
                                i + 1, players[i].name, line);

                }
            }
        }

        /* ----- apply turn ----- */

        int chosen_row[MAX_PLAYERS];
        int taken_row[MAX_PLAYERS];
        int bulls[MAX_PLAYERS];

        for (int i = 0; i < port_players; i++)
            chosen_row[i] = players[i].chosen_row;

        game_apply_turn(&game, chosen_row, taken_row, bulls);

        for (int i = 0; i < port_players; i++) {
            if (taken_row[i] >= 0) {
                printf("J%d (%s) ramasse R%d (+%d)\n",
                       i + 1, players[i].name,
                       taken_row[i] + 1, bulls[i]);
            }
        }

        char score[LINE_MAX];
        game_score_string(&game, score, sizeof(score));
        printf("SCORES: %s\n", score);
        broadcast(players, port_players, score);
    }

    printf("\nFIN DE PARTIE\n");

    for (int i = 0; i < port_players; i++) {
        fclose(players[i].in);
        fclose(players[i].out);
        close(players[i].fd);
    }

    close(listen_fd);
    return 0;
}
