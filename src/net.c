#include "headers/net.h"
#include "headers/util.h"

// Utilise SO_REUSEADDR pour redémarrer un socket sans délai.
static int set_reuseaddr(int fd) {
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0;
}

// Met en place un socket d'écoute IPv4 simple comme vu en cours.
int tcp_listen(const char *port) {
    struct addrinfo hints, *res, *p;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) return -1;

    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        set_reuseaddr(fd);
        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
            if (listen(fd, 32) == 0) break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

// Se connecte à un hôte distant en mode bloquant.
int tcp_connect(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

// Fournit un FILE* tamponné en lecture autour du socket.
FILE *fdopen_r(int fd) {
    FILE *f = fdopen(dup(fd), "r");
    if (!f) return NULL;
    setvbuf(f, NULL, _IOLBF, 0);
    return f;
}

// Fournit un FILE* tamponné en écriture autour du socket.
FILE *fdopen_w(int fd) {
    FILE *f = fdopen(dup(fd), "w");
    if (!f) return NULL;
    setvbuf(f, NULL, _IOLBF, 0);
    return f;
}

// Envoie une ligne terminée par \n via le socket.
int send_line(FILE *out, const char *line) {
    if (fprintf(out, "%s\n", line) < 0) return 0;
    fflush(out);
    return 1;
}

// Lit une ligne et supprime les fins CR/LF éventuelles.
int recv_line(FILE *in, char *buf, int cap) {
    if (!fgets(buf, cap, in)) return 0;
    trim_crlf(buf);
    return 1;
}
