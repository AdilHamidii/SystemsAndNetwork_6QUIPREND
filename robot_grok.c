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

static int choose_card_fallback(int *hand, int hn, Row rows[ROWS]) {
    int bestc = hand[0];
    int bestrisk = 0x7fffffff;
    for (int i = 0; i < hn; i++) {
        int c = hand[i];
        int r = best_row_for_card_local(rows, c);
        int risk = 0;
        if (r < 0) risk = 10000 + bulls(c);
        else {
            int len = rows[r].len;
            if (len == ROW_MAX) risk = 5000 + row_bulls_local(&rows[r]);
            else risk = (c - rows[r].cards[len - 1]) + (len * 10);
        }
        if (risk < bestrisk) { bestrisk = risk; bestc = c; }
    }
    return bestc;
}

static int grok_pick_card(const char *apikey, int *hand, int hn, Row rows[ROWS]) {
    char handbuf[512];
    handbuf[0] = 0;
    for (int i = 0; i < hn; i++) {
        char t[32];
        snprintf(t, sizeof(t), "%d", hand[i]);
        strncat(handbuf, t, sizeof(handbuf) - (int)strlen(handbuf) - 1);
        if (i + 1 < hn) strncat(handbuf, " ", sizeof(handbuf) - (int)strlen(handbuf) - 1);
    }

    char tablebuf[512];
    tablebuf[0] = 0;
    for (int r = 0; r < ROWS; r++) {
        char t[128];
        snprintf(t, sizeof(t), "R%d:", r+1);
        strncat(tablebuf, t, sizeof(tablebuf) - (int)strlen(tablebuf) - 1);
        for (int i = 0; i < rows[r].len; i++) {
            snprintf(t, sizeof(t), " %d", rows[r].cards[i]);
            strncat(tablebuf, t, sizeof(tablebuf) - (int)strlen(tablebuf) - 1);
        }
        if (r + 1 < ROWS) strncat(tablebuf, " | ", sizeof(tablebuf) - (int)strlen(tablebuf) - 1);
    }

    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
             "Tu joues a 6 qui prend. Reponds UNIQUEMENT un entier present dans la main.\n"
             "Main: %s\nTable: %s\n"
             "Choisis la meilleure carte pour minimiser les tetes de boeuf, sans autre texte.",
             handbuf, tablebuf);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "curl -s https://api.x.ai/v1/chat/completions "
             "-H 'Content-Type: application/json' "
             "-H 'Authorization: Bearer %s' "
             "-d '{\"model\":\"grok\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.2,\"max_tokens\":10}'",
             apikey, prompt);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    char resp[8192];
    size_t n = fread(resp, 1, sizeof(resp)-1, fp);
    resp[n] = 0;
    pclose(fp);

    char *p = strstr(resp, "\"content\"");
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    while (*p && (*p == ':' || *p == ' ')) p++;
    while (*p && (*p == '\"' || *p == ' ')) p++;
    int v = atoi(p);
    if (v <= 0) return -1;

    for (int i = 0; i < hn; i++) if (hand[i] == v) return v;
    return -1;
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
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <host> <port> <pseudo> <XAI_API_KEY>\n", argv[0]);
        return 1;
    }

    const char *apikey = argv[4];

    int fd = tcp_connect(argv[1], argv[2]);
    if (fd < 0) die("connect");

    FILE *in = fdopen_r(fd);
    FILE *out = fdopen_w(fd);
    if (!in || !out) die("fdopen");

    send_line(out, argv[3]);

    int hand[HAND_SIZE];
    int hn = 0;
    Row rows[ROWS];
    for (int r = 0; r < ROWS; r++) { rows[r].len = 1; rows[r].cards[0] = 1; }

    char line[LINE_MAX];
    while (recv_line(in, line, sizeof(line))) {
        if (str_starts(line, "ETAT ") || str_starts(line, "TABLE:") || str_starts(line, "TABLE ")) {
            parse_table_rows(line, rows);
            continue;
        }

        if (str_starts(line, "MAIN ")) {
            hn = parse_hand(line, hand, HAND_SIZE);
            continue;
        }

        if (strcmp(line, "DEMANDE_CARTE") == 0 || str_starts(line, "DEMANDE_CARTE")) {
            int c = -1;
            if (hn > 0) c = grok_pick_card(apikey, hand, hn, rows);
            if (c < 0 && hn > 0) c = choose_card_fallback(hand, hn, rows);
            if (c < 0) c = 1;

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

