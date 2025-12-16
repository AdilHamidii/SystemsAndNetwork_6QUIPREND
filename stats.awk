BEGIN {
    order_count = 0;
}

$1=="TURN" && $4=="PLAY" {
    player = $3;
    plays[player]++;
    if (!(player in seen)) {
        order[++order_count] = player;
        seen[player] = 1;
    }
}

$1=="TURN" && $4=="TAKE" {
    bulls[$3] += $7
}

$1=="SCORE" {
    for (i=2;i<=NF;i++) {
        split($i,a,"=")
        last[a[1]] = a[2]
    }
}

END {
    print "STATISTIQUES DE LA PARTIE"
    for (i = 1; i <= order_count; i++) {
        p = order[i]
        printf("%s : tours=%d boeufs=%d score_final=%d\n",
               p, plays[p], bulls[p], last[p])
    }
}
