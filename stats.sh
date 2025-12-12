#!/bin/sh
LOG=${1:-logs/partie.log}
awk -f stats.awk "$LOG" > logs/stats.txt
pdflatex -interaction=nonstopmode -output-directory=logs rapport.tex >/dev/null 2>&1
echo "OK: logs/stats.txt et logs/rapport.pdf"
