#include "common.h"
#include "net.h"
#include "util.h"
#include "game.h"

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

static void parse_table_rows(const char *line, Row rows[ROWS]) {
    for (int r = 0; r < ROWS; r++) { rows[r].len = 0; }
    const char *p = line;
    while (*p && *p != '|') p++;
    p = line;
    for (int r = 0; r < ROWS; r++) {
        while (*p && *p != ':') p++;
        if (!*p) break;
        p++;
        while (*p == ' ') p++;
        while (*p && *p != '|') {
            int v = atoi(p);
            if (v > 0 && rows[r].len < ROW_MAX) rows[r].cards[rows[r].len++] = v;
            while (*p && *p != ' ' && *p != '|') p++;
            while (*p == ' ') p++;
        }
        while (*p && *p != 'R' && *p != 0) p++;
    }
}

static int row_bulls_local(Row *r) {
    int s = 0;
    for (int i = 0; i < r->len; i++) s += bulls(r->cards[i]);
    return s;
}

static int choose_row_min_bulls(Row rows[ROWS]) {
    int best = 0;
    int bestv = row_bulls_local(&rows[0]);
    for (int r = 1; r < ROWS; r++) {
        int v = row_bulls_local(&rows[r]);
        if (v < bestv) { bestv = v; best = r; }
    }
    return best;
}

static int best_row_for_card_local(Row rows[ROWS], int c) {
    int best = -1;
    int bestdiff = 0x7fffffff;
    for (int r = 0; r < ROWS; r++) {
        int last = rows[r].cards[rows[r].len - 1];
        if (c > last) {
            int d = c - last;
            if (d < bestdiff) { bestdiff = d; best = r; }
        }
    }
    return best;
}

static int choose_card_heuristic(int *hand, int hn, Row rows[ROWS]) {
    int bestc = hand[0];
    int bestrisk = 0x7fffffff;

    for (int i = 0; i < hn; i++) {
        int c = hand[i];
        int r = best_row_for_card_local(rows, c);
        int risk = 0;
        if (r < 0) {
            risk = 10000 + bulls(c);
        } else {
            int len = rows[r].len;
            if (len == ROW_MAX) {
                risk = 5000 + row_bulls_local(&rows[r]);
            } else {
                risk = (c - rows[r].cards[len - 1]) + (len * 10);
            }
        }
        if (risk < bestrisk) { bestrisk = risk; bestc = c; }
    }

    return bestc;
}

static void remove_from_hand(int *hand, int *hn, int c) {
    int n = *hn;
    for (int i = 0; i < n; i++) {
        if (hand[i] == c) {
            for (int j = i + 1; j < n; j++) hand[j-1] = hand[j];
            *hn = n - 1;
            return;
        }
    }
}

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

    int hand[HAND_SIZE];
    int hn = 0;
    Row rows[ROWS];
    for (int r = 0; r < ROWS; r++) rows[r].len = 1, rows[r].cards[0] = 1;

    char line[LINE_MAX];
    while (recv_line(in, line, sizeof(line))) {
        if (str_starts(line, "BIENVENUE ")) {
            send_line(out, "INFO Robot connecte");
            char buf[LINE_MAX];
            snprintf(buf, sizeof(buf), "NOM %s", argv[3]);
            send_line(out, buf);
            continue;
        }

        if (str_starts(line, "ETAT ")) {
            parse_table_rows(line, rows);
            continue;
        }

        if (str_starts(line, "MAIN ")) {
            hn = parse_hand(line, hand, HAND_SIZE);
            continue;
        }

        if (str_starts(line, "DEMANDE_CARTE")) {
            int c = choose_card_heuristic(hand, hn, rows);
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "JOUER %d", c);
            send_line(out, cmd);
            remove_from_hand(hand, &hn, c);
            continue;
        }

        if (str_starts(line, "CHOISIR_RANGEES")) {
            int r = choose_row_min_bulls(rows) + 1;
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "RANGE %d", r);
            send_line(out, cmd);
            continue;
        }
    }

    fclose(in);
    fclose(out);
    close(fd);
    return 0;
}
