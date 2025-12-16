#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"
#include "headers/game.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>



static int global_game_id = 0;
static pthread_mutex_t game_id_lock = PTHREAD_MUTEX_INITIALIZER;



typedef struct {
    int fd;
    FILE *in;
    FILE *out;
    char name[PLAYER_NAME_MAX];
} WaitingPlayer;

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

typedef struct {
    int game_id;
    int nplayers;
    WaitingPlayer wp[MAX_PLAYERS];
} GameArgs;



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

static int parse_play(const char *line, int *out) {
    while (*line && isspace((unsigned char)*line)) line++;

    if (isdigit((unsigned char)*line))
        return parse_int(line, out);

    if (strncasecmp(line, "JOUER", 5) == 0) {
        line += 5;
        while (*line && isspace((unsigned char)*line)) line++;
        return parse_int(line, out);
    }

    return 0;
}

static int needs_row(Game *g, int c) {
    for (int r = 0; r < ROWS; r++)
        if (c > g->rows[r].cards[g->rows[r].len - 1])
            return 0;
    return 1;
}

static void close_player(Player *p) {
    if (p->in) fclose(p->in);
    if (p->out) fclose(p->out);
    if (p->fd >= 0) close(p->fd);
    p->connected = 0;
}



static void *game_thread(void *arg) {
    GameArgs *ga = arg;
    int gid = ga->game_id;
    int n = ga->nplayers;

    printf("[PARTIE %d] Thread demarre (%d joueurs)\n", gid, n);

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));

    for (int i = 0; i < n; i++) {
        players[i].fd = ga->wp[i].fd;
        players[i].in = ga->wp[i].in;
        players[i].out = ga->wp[i].out;
        players[i].connected = 1;
        strncpy(players[i].name, ga->wp[i].name, PLAYER_NAME_MAX - 1);

        printf("[PARTIE %d] Joueur %d = %s\n",
               gid, i + 1, players[i].name);
    }

    free(ga);

    Game game;
    game_init(&game, n);
    game_setup_rows(&game);
    game_deal(&game);

    broadcast(players, n, "INFO Partie demarree.");

    while (!game_over(&game, 66)) {
        char table[LINE_MAX];
        game_table_string(&game, table, sizeof(table));

        printf("[PARTIE %d] TABLE: %s\n", gid, table);
        broadcast(players, n, table);

        for (int i = 0; i < n; i++) {
            char hand[LINE_MAX];
            game_hand_string(&game, i, hand, sizeof(hand));
            sendf(players[i].out, "MAIN %s", hand);
            players[i].played = 0;
            players[i].chosen_row = -1;
        }

     

        for (int i = 0; i < n; i++) {
            char line[LINE_MAX];
            int c;

            for (;;) {
                send_line(players[i].out, "DEMANDE_CARTE");
                if (!recv_line(players[i].in, line, sizeof(line))) {
                    printf("[PARTIE %d] Joueur %d deconnecte\n", gid, i + 1);
                    goto end_game;
                }

                if (!parse_play(line, &c) || !game_hand_has(&game, i, c)) {
                    send_line(players[i].out, "ERREUR Carte invalide.");
                    continue;
                }

                game_hand_remove(&game, i, c);
                game.carte_jouee[i] = c;
                players[i].card = c;

                printf("[PARTIE %d] Joueur %d (%s) joue %d\n",
                       gid, i + 1, players[i].name, c);
                break;
            }
        }

       

        int order[MAX_PLAYERS];
        for (int i = 0; i < n; i++) order[i] = i;

        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (game.carte_jouee[order[j]] < game.carte_jouee[order[i]]) {
                    int t = order[i];
                    order[i] = order[j];
                    order[j] = t;
                }

        int taken[MAX_PLAYERS];
        int bulls[MAX_PLAYERS];
        memset(taken, -1, sizeof(taken));
        memset(bulls, 0, sizeof(bulls));

       

        for (int k = 0; k < n; k++) {
            int pid = order[k];
            int c = game.carte_jouee[pid];

            if (needs_row(&game, c)) {
                char line[LINE_MAX];
                for (;;) {
                    send_line(players[pid].out, "CHOISIR_RANGEES");
                    if (!recv_line(players[pid].in, line, sizeof(line)))
                        goto end_game;

                    int r;
                    if (parse_int(line, &r) && r >= 1 && r <= ROWS) {
                        players[pid].chosen_row = r - 1;
                        break;
                    }
                    send_line(players[pid].out, "ERREUR Choix de rangee invalide.");
                }
            }

            game_place_card(&game, pid, c,
                            players[pid].chosen_row,
                            &taken[pid], &bulls[pid]);

            if (taken[pid] >= 0) {
                printf("[PARTIE %d] Joueur %d (%s) ramasse la rangee %d (+%d)\n",
                       gid, pid + 1, players[pid].name,
                       taken[pid] + 1, bulls[pid]);
            }
        }

        char score[LINE_MAX];
        game_score_string(&game, score, sizeof(score));
        printf("[PARTIE %d] SCORES: %s\n", gid, score);
        broadcast(players, n, score);
    }

end_game:
    printf("[PARTIE %d] Fin de la partie\n", gid);

    for (int i = 0; i < n; i++)
        close_player(&players[i]);

    return NULL;
}

 

static WaitingPlayer waitq[256];
static int waitq_count = 0;
static pthread_mutex_t waitq_lock = PTHREAD_MUTEX_INITIALIZER;



int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <joueurs_par_partie>\n", argv[0]);
        return 1;
    }

    int joueurs_par_partie = atoi(argv[2]);
    int listen_fd = tcp_listen(argv[1]);

    printf("Serveur: ecoute sur le port %s, joueurs_par_partie=%d\n",
           argv[1], joueurs_par_partie);

    while (1) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int fd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (fd < 0) continue;

        FILE *in = fdopen_r(fd);
        FILE *out = fdopen_w(fd);

        char name[PLAYER_NAME_MAX];
        recv_line(in, name, sizeof(name));

        printf("Connexion: (%s) depuis %s (en attente)\n",
               name, inet_ntoa(cli.sin_addr));

        pthread_mutex_lock(&waitq_lock);

        waitq[waitq_count].fd = fd;
        waitq[waitq_count].in = in;
        waitq[waitq_count].out = out;
        strncpy(waitq[waitq_count].name, name, PLAYER_NAME_MAX - 1);
        waitq_count++;

        while (waitq_count >= joueurs_par_partie) {
            pthread_mutex_lock(&game_id_lock);
            int gid = ++global_game_id;
            pthread_mutex_unlock(&game_id_lock);

            printf("[PARTIE %d] Creation de la partie\n", gid);

            GameArgs *ga = malloc(sizeof(GameArgs));
            ga->game_id = gid;
            ga->nplayers = joueurs_par_partie;

            for (int i = 0; i < joueurs_par_partie; i++)
                ga->wp[i] = waitq[i];

            for (int i = joueurs_par_partie; i < waitq_count; i++)
                waitq[i - joueurs_par_partie] = waitq[i];

            waitq_count -= joueurs_par_partie;

            pthread_t tid;
            pthread_create(&tid, NULL, game_thread, ga);
            pthread_detach(tid);
        }

        pthread_mutex_unlock(&waitq_lock);
    }

    close(listen_fd);
    return 0;
}
