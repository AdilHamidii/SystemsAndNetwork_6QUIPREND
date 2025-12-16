#include <stdarg.h>
#include <ctype.h>
#include "headers/common.h"
#include "headers/net.h"
#include "headers/util.h"

static void sendf(FILE *out, const char *fmt, ...) {
    char buf[LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    send_line(out, buf);
}

static int looks_like_int(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') s++;
    if (!isdigit((unsigned char)*s)) return 0;
    while (*s && isdigit((unsigned char)*s)) s++;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == 0;
}

static int looks_like_play(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (looks_like_int(s)) return 1;
    if (tolower((unsigned char)s[0])=='j' &&
        tolower((unsigned char)s[1])=='o' &&
        tolower((unsigned char)s[2])=='u' &&
        tolower((unsigned char)s[3])=='e' &&
        tolower((unsigned char)s[4])=='r') return 1;
    return 0;
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

    send_line(out, argv[3]);

    char line[LINE_MAX];
    while (recv_line(in, line, sizeof(line))) {
        printf("%s\n", line);

        if (strcmp(line, "DEMANDE_CARTE") == 0) {
            char cmd[64];
            for (;;) {
                printf("> ");
                fflush(stdout);
                if (!fgets(cmd, sizeof(cmd), stdin)) return 0;
                trim_crlf(cmd);
                if (looks_like_play(cmd)) break;
                printf("Commande invalide. Exemple: JOUER 42 (ou juste 42)\n");
            }
            send_line(out, cmd);
        }

        if (str_starts(line, "CHOISIR_RANGEES")) {
            char cmd[64];
            for (;;) {
                printf("> ");
                fflush(stdout);
                if (!fgets(cmd, sizeof(cmd), stdin)) return 0;
                trim_crlf(cmd);
                if (looks_like_int(cmd)) break;
                printf("Choix invalide. Entrez un numero (1-4)\n");
            }
            send_line(out, cmd);
        }
    }

    fclose(in);
    fclose(out);
    close(fd);
    return 0;
}
