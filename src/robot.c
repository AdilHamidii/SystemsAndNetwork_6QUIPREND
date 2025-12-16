#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"
#include "headers/game.h"

// Extrait les valeurs entières de la main envoyée par le serveur.
static int parse_hand(const char *line, int *cards, int cap) {
    int n = 0;
    const char *p = line;

    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    while (*p && n < cap) {
        int v = atoi(p);
        if (v > 0) cards[n++] = v;
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
    }
    return n;
}

// Reconstruit les quatre rangées décrites dans la ligne R1:/R2:.
static void parse_table_rows(const char *line, Row rows[ROWS]) {
    for (int r = 0; r < ROWS; r++) rows[r].len = 0;

    const char *p = line;
    for (int r = 0; r < ROWS; r++) {
        while (*p && *p != ':') p++;
        if (!*p) break;
        p++;
        while (*p == ' ') p++;

        while (*p && *p != '|') {
            int v = atoi(p);
            if (v > 0 && rows[r].len < ROW_MAX)
                rows[r].cards[rows[r].len++] = v;

            while (*p && *p != ' ' && *p != '|') p++;
            while (*p == ' ') p++;
        }
        while (*p && *p != 'R')
            p++;
    }
}

// Additionne les têtes de bœuf présentes dans une rangée.
static int row_bulls_local(Row *r) {
    int s = 0;
    for (int i = 0; i < r->len; i++)
        s += bulls(r->cards[i]);
    return s;
}

// Choisit la rangée la moins pénalisante lorsqu'on doit ramasser.
static int choose_row_min_bulls(Row rows[ROWS]) {
    int best = 0;
    int bestv = row_bulls_local(&rows[0]);
    for (int r = 1; r < ROWS; r++) {
        int v = row_bulls_local(&rows[r]);
        if (v < bestv) {
            bestv = v;
            best = r;
        }
    }
    return best;
}

// Renvoie la plus petite carte encore disponible dans la main.
static int choose_smallest_card(int *hand, int hn) {
    if (hn <= 0) return -1;
    int m = hand[0];
    for (int i = 1; i < hn; i++)
        if (hand[i] < m)
            m = hand[i];
    return m;
}

// Retire une carte déjà jouée tout en compactant la main locale.
static void remove_from_hand(int *hand, int *hn, int c) {
    for (int i = 0; i < *hn; i++) {
        if (hand[i] == c) {
            for (int j = i + 1; j < *hn; j++)
                hand[j - 1] = hand[j];
            (*hn)--;
            return;
        }
    }
}

// Client automatique minimaliste qui suit le protocole texte.
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <pseudo>\n", argv[0]);
        return 1;
    }

    int fd = tcp_connect(argv[1], argv[2]);
    if (fd < 0) die("connect");

    FILE *in = fdopen_r(fd);
    FILE *out = fdopen_w(fd);
    if (!in || !out) die("fdopen");

    send_line(out, argv[3]);

    int hand[HAND_SIZE];
    int hn = 0;
    Row rows[ROWS];
    memset(rows, 0, sizeof(rows));

    char line[LINE_MAX];
    while (recv_line(in, line, sizeof(line))) {
        if (str_starts(line, "R1:")) {
            parse_table_rows(line, rows);
            continue;
        }

        if (str_starts(line, "MAIN ")) {
            hn = parse_hand(line, hand, HAND_SIZE);
            continue;
        }

        if (strcmp(line, "DEMANDE_CARTE") == 0) {
            int c = choose_smallest_card(hand, hn);
            if (c < 0) c = 0;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "JOUER %d", c);
            send_line(out, cmd);
            remove_from_hand(hand, &hn, c);
            continue;
        }

        if (strcmp(line, "CHOISIR_RANGEES") == 0) {
            int r = choose_row_min_bulls(rows) + 1;
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "%d", r);
            send_line(out, cmd);
            continue;
        }
    }

    fclose(in);
    fclose(out);
    close(fd);
    return 0;
}
