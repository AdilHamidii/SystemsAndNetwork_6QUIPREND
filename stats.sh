#!/bin/sh

set -e

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)

LOG="$1"
if [ -z "$LOG" ]; then
    echo "Usage: ./stats.sh <path/to/log>"
    exit 1
fi

if [ ! -f "$LOG" ]; then
    if [ -f "$ROOT_DIR/$LOG" ]; then
        LOG="$ROOT_DIR/$LOG";
    else
        echo "Log file not found: $LOG"
        exit 1
    fi
fi

LOGDIR=$(cd "$(dirname "$LOG")" && pwd)
LOGFILE="$LOGDIR/$(basename "$LOG")"

STATS_TXT="$LOGDIR/stats.txt"
awk -f "$ROOT_DIR/stats.awk" "$LOGFILE" > "$STATS_TXT"

(cd "$ROOT_DIR" && pdflatex -interaction=nonstopmode rapport.tex > /dev/null)
