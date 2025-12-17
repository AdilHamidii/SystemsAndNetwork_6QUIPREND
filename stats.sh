#!/bin/sh

set -e

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
# Vérifie les arguments.
LOG="$1"
if [ -z "$LOG" ]; then
    echo "Usage: ./stats.sh <path/to/log>"
    exit 1
fi
# Vérifie si le fichier de log existe.
if [ ! -f "$LOG" ]; then
    if [ -f "$ROOT_DIR/$LOG" ]; then
        LOG="$ROOT_DIR/$LOG";
    else
        echo "Log file not found: $LOG"
        exit 1
    fi
fi
# Résout le chemin absolu du fichier de log.
LOGDIR=$(cd "$(dirname "$LOG")" && pwd)
LOGFILE="$LOGDIR/$(basename "$LOG")"
# Génère les statistiques à partir du fichier de log.
STATS_TXT="$LOGDIR/stats.txt"
awk -f "$ROOT_DIR/stats.awk" "$LOGFILE" > "$STATS_TXT"
# Génère le rapport PDF à partir des statistiques.
(cd "$ROOT_DIR" && pdflatex -interaction=nonstopmode rapport.tex > /dev/null)
