#include "headers/util.h"
// Affiche un message d'erreur et termine le programme.
void die(const char *m) {
    perror(m);
    exit(1);
}
// Vérifie si une chaîne commence par un préfixe donné. 
int str_starts(const char *s, const char *p) {
    size_t n = strlen(p);
    return strncmp(s, p, n) == 0;
}
// Convertit une chaîne en entier avec validation complète. 
int parse_int(const char *s, int *out) {
    char *e = 0;
    long v = strtol(s, &e, 10);
    if (!e || *e != 0) return 0;
    if (v < -2147483648L || v > 2147483647L) return 0;
    *out = (int)v;
    return 1;
}
// Supprime les caractères de fin de ligne d'une chaîne. pour I/O du txte.
void trim_crlf(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = 0;
        n--;
    }
}
