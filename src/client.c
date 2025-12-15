#include <stdarg.h>
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

    /* SEND NAME ONCE */
    send_line(out, argv[3]);

    char line[LINE_MAX];
    while (recv_line(in, line, sizeof(line))) {
        printf("%s\n", line);

        if (strcmp(line, "DEMANDE_CARTE") == 0) {
            char cmd[64];
            printf("> ");
            fflush(stdout);
            fgets(cmd, sizeof(cmd), stdin);
            trim_crlf(cmd);
            send_line(out, cmd);
        }

        if (str_starts(line, "CHOISIR_RANGEES")) {
            char cmd[64];
            printf("> ");
            fflush(stdout);
            fgets(cmd, sizeof(cmd), stdin);
            trim_crlf(cmd);
            send_line(out, cmd);
        }
    }

    fclose(in);
    fclose(out);
    close(fd);
    return 0;
}

