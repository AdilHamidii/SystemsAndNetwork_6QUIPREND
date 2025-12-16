#define _POSIX_C_SOURCE 200112L

#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"
#include "headers/game.h"

#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

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
    int card;
    int chosen_row;
} Player;

typedef struct {
    int game_id;
    int nplayers;
    WaitingPlayer wp[MAX_PLAYERS];
} GameArgs;

static WaitingPlayer waitq[256];
static int waitq_count = 0;
static pthread_mutex_t waitq_lock = PTHREAD_MUTEX_INITIALIZER;

// Formate et envoie une ligne sur un flux client.
static void sendf(FILE *out, const char *fmt, ...) {
    char buf[LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    send_line(out, buf);
}

// Propagation atomique d'un message à tous les joueurs connectés.
static void broadcast(Player *p, int n, const char *msg) {
    for (int i = 0; i < n; i++)
        if (p[i].connected)
            send_line(p[i].out, msg);
}

// Analyse sécurisée d'une commande JOUER envoyée par un client.
static int parse_play(const char *line, int *out) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (strncasecmp(line, "JOUER", 5) == 0) {
        line += 5;
        while (*line && isspace((unsigned char)*line)) line++;
    }
    if (!parse_int(line, out)) return 0;
    if (*out <= 0) return 0;
    return 1;
}

// Détermine si une carte doit obligatoirement prendre une rangée.
static int needs_row(Game *g, int c) {
    for (int r = 0; r < ROWS; r++)
        if (c > g->rows[r].cards[g->rows[r].len - 1])
            return 0;
    return 1;
}

// Libère toutes les ressources associées à un joueur.
static void close_player(Player *p) {
    if (!p->connected) return;
    if (p->in) fclose(p->in);
    if (p->out) fclose(p->out);
    if (p->fd >= 0) close(p->fd);
    p->in = NULL;
    p->out = NULL;
    p->fd = -1;
    p->connected = 0;
}

// Ajoute une ligne au journal de partie si disponible.
static void logf_line(FILE *lf, const char *fmt, ...) {
    if (!lf) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(lf, fmt, ap);
    va_end(ap);
    fflush(lf);
}

// Boucle principale gérant une partie complète dans un thread dédié.
static void *game_thread(void *arg) {
    GameArgs *ga = arg;
    int gid = ga->game_id;
    int n = ga->nplayers;

    char logfile[128];
    snprintf(logfile, sizeof(logfile), "logs/partie_%d.log", gid);
    FILE *lf = fopen(logfile, "w");

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));

    for (int i = 0; i < n; i++) {
        players[i].fd = ga->wp[i].fd;
        players[i].in = ga->wp[i].in;
        players[i].out = ga->wp[i].out;
        players[i].connected = 1;
        strncpy(players[i].name, ga->wp[i].name, PLAYER_NAME_MAX - 1);
        players[i].name[PLAYER_NAME_MAX - 1] = 0;
        players[i].card = -1;
        players[i].chosen_row = -1;
    }

    free(ga);

    printf("[PARTIE %d] Demarrage (%d joueurs)\n", gid, n);
    for (int i = 0; i < n; i++)
        printf("[PARTIE %d] Joueur %d = %s\n", gid, i + 1, players[i].name);

    logf_line(lf, "PARTIE %d DEBUT\n", gid);
    logf_line(lf, "JOUEURS ");
    for (int i = 0; i < n; i++) logf_line(lf, "%d:%s ", i + 1, players[i].name);
    logf_line(lf, "\n");

    if (!lf) {
        printf("[PARTIE %d] Impossible d'ouvrir %s (errno=%d). La partie continue sans fichier log.\n",
               gid, logfile, errno);
    }

    Game game;
    game_init(&game, n);
    game_setup_rows(&game);
    game_deal(&game);

    sendf(players[0].out, "INFO Partie %d demarree.", gid);
    for (int i = 1; i < n; i++) sendf(players[i].out, "INFO Partie %d demarree.", gid);

    while (!game_over(&game, 66)) {
        char table[LINE_MAX];
        game_table_string(&game, table, sizeof(table));
        broadcast(players, n, table);

        printf("[PARTIE %d] TOUR %d TABLE: %s\n", gid, game.tour, table);
        logf_line(lf, "TOUR %d TABLE %s\n", game.tour, table);

        for (int i = 0; i < n; i++) {
            char hand[LINE_MAX];
            game_hand_string(&game, i, hand, sizeof(hand));
            sendf(players[i].out, "MAIN %s", hand);
            players[i].chosen_row = -1;
            players[i].card = -1;
        }

        for (int i = 0; i < n; i++) {
            char line[LINE_MAX];
            int c;

            for (;;) {
                send_line(players[i].out, "DEMANDE_CARTE");
                if (!recv_line(players[i].in, line, sizeof(line))) {
                    printf("[PARTIE %d] Joueur %d (%s) deconnecte pendant DEMANDE_CARTE\n",
                           gid, i + 1, players[i].name);
                    logf_line(lf, "DECO JOUEUR %d %s\n", i + 1, players[i].name);
                    goto end;
                }

                if (!parse_play(line, &c) || !game_hand_has(&game, i, c)) {
                    send_line(players[i].out, "ERREUR Carte invalide");
                    continue;
                }

                if (!game_hand_remove(&game, i, c)) {
                    send_line(players[i].out, "ERREUR Carte invalide");
                    continue;
                }

                game.carte_jouee[i] = c;
                players[i].card = c;

                printf("[PARTIE %d] TOUR %d Joueur %d (%s) joue %d\n",
                       gid, game.tour, i + 1, players[i].name, c);
                logf_line(lf, "TOUR %d PLAY %d %s %d\n", game.tour, i + 1, players[i].name, c);
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
        for (int i = 0; i < n; i++) { taken[i] = -1; bulls[i] = 0; }

        for (int k = 0; k < n; k++) {
            int pid = order[k];
            int c = game.carte_jouee[pid];

            if (needs_row(&game, c)) {
                char line[LINE_MAX];
                printf("[PARTIE %d] TOUR %d Joueur %d (%s) doit choisir une rangee\n",
                       gid, game.tour, pid + 1, players[pid].name);
                logf_line(lf, "TOUR %d NEED_ROW %d %s\n", game.tour, pid + 1, players[pid].name);

                for (;;) {
                    send_line(players[pid].out, "CHOISIR_RANGEES");
                    if (!recv_line(players[pid].in, line, sizeof(line))) {
                        printf("[PARTIE %d] Joueur %d (%s) deconnecte pendant CHOISIR_RANGEES\n",
                               gid, pid + 1, players[pid].name);
                        logf_line(lf, "DECO JOUEUR %d %s\n", pid + 1, players[pid].name);
                        goto end;
                    }
                    int r;
                    if (parse_int(line, &r) && r >= 1 && r <= ROWS) {
                        players[pid].chosen_row = r - 1;
                        printf("[PARTIE %d] TOUR %d Joueur %d (%s) choisit rangee %d\n",
                               gid, game.tour, pid + 1, players[pid].name, r);
                        logf_line(lf, "TOUR %d CHOOSE_ROW %d %s %d\n", game.tour, pid + 1, players[pid].name, r);
                        break;
                    }
                    send_line(players[pid].out, "ERREUR Choix de rangee invalide");
                }
            }

            game_place_card(&game, pid, c, players[pid].chosen_row, &taken[pid], &bulls[pid]);

            if (taken[pid] >= 0) {
                printf("[PARTIE %d] TOUR %d Joueur %d (%s) ramasse rangee %d (+%d)\n",
                       gid, game.tour, pid + 1, players[pid].name, taken[pid] + 1, bulls[pid]);
                logf_line(lf, "TOUR %d TAKE %d %s ROW %d BULLS %d\n",
                          game.tour, pid + 1, players[pid].name, taken[pid] + 1, bulls[pid]);
            }
        }

        char score[LINE_MAX];
        game_score_string(&game, score, sizeof(score));
        broadcast(players, n, score);

        printf("[PARTIE %d] TOUR %d SCORES: %s\n", gid, game.tour, score);
        logf_line(lf, "TOUR %d SCORES %s\n", game.tour, score);

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

end:
    printf("[PARTIE %d] Fin de la partie\n", gid);
    logf_line(lf, "PARTIE %d FIN\n", gid);
    if (lf) fclose(lf);

    for (int i = 0; i < n; i++)
        close_player(&players[i]);

    return NULL;
}

// Point d'entrée du serveur: accepte les connexions et lance les parties.
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <joueurs_par_partie>\n", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    int joueurs_par_partie = atoi(argv[2]);
    if (joueurs_par_partie < 2 || joueurs_par_partie > MAX_PLAYERS)
        die("Nombre de joueurs par partie invalide");

    int listen_fd = tcp_listen(argv[1]);
    if (listen_fd < 0) die("listen");

    if (mkdir("logs", 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "Erreur mkdir logs: %d\n", errno);
        return 1;
    }

    printf("Serveur: ecoute sur le port %s, joueurs_par_partie=%d\n",
           argv[1], joueurs_par_partie);

    while (1) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);

        int fd = accept(listen_fd, (struct sockaddr *)&cli, &len);
        if (fd < 0) continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        printf("Connexion TCP entrante depuis %s\n", ip);
        fflush(stdout);

        FILE *in = fdopen_r(fd);
        FILE *out = fdopen_w(fd);
        if (!in || !out) {
            if (in) fclose(in);
            if (out) fclose(out);
            close(fd);
            continue;
        }

        char name[PLAYER_NAME_MAX];
        if (!recv_line(in, name, sizeof(name))) {
            printf("Connexion abandonnee avant envoi du pseudo (%s)\n", ip);
            fclose(in);
            fclose(out);
            close(fd);
            continue;
        }

        pthread_mutex_lock(&waitq_lock);

        if (waitq_count >= (int)(sizeof(waitq) / sizeof(waitq[0]))) {
            pthread_mutex_unlock(&waitq_lock);
            send_line(out, "INFO Serveur complet. Reessayez plus tard.");
            fclose(in);
            fclose(out);
            close(fd);
            continue;
        }

        waitq[waitq_count].fd = fd;
        waitq[waitq_count].in = in;
        waitq[waitq_count].out = out;
        strncpy(waitq[waitq_count].name, name, PLAYER_NAME_MAX - 1);
        waitq[waitq_count].name[PLAYER_NAME_MAX - 1] = 0;
        waitq_count++;

        printf("Connexion: (%s) depuis %s (en attente=%d)\n", name, ip, waitq_count);

        while (waitq_count >= joueurs_par_partie) {
            pthread_mutex_lock(&game_id_lock);
            int gid = ++global_game_id;
            pthread_mutex_unlock(&game_id_lock);

            GameArgs *ga = malloc(sizeof(GameArgs));
            if (!ga) break;

            ga->game_id = gid;
            ga->nplayers = joueurs_par_partie;

            for (int i = 0; i < joueurs_par_partie; i++)
                ga->wp[i] = waitq[i];

            for (int i = joueurs_par_partie; i < waitq_count; i++)
                waitq[i - joueurs_par_partie] = waitq[i];

            waitq_count -= joueurs_par_partie;

            printf("[PARTIE %d] Creation (%d joueurs). Reste en attente=%d\n",
                   gid, joueurs_par_partie, waitq_count);

            pthread_t tid;
            if (pthread_create(&tid, NULL, game_thread, ga) == 0) {
                pthread_detach(tid);
            } else {
                for (int i = 0; i < joueurs_par_partie; i++) {
                    fclose(ga->wp[i].in);
                    fclose(ga->wp[i].out);
                    close(ga->wp[i].fd);
                }
                free(ga);
            }
        }

        pthread_mutex_unlock(&waitq_lock);
    }

    return 0;
}
