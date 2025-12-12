BEGIN{
    FS=" "
}
$1=="TOUR"{
    for(i=1;i<=NF;i++){
        if($i ~ /^NOM=/){ split($i,a,"="); nom=a[2] }
        if($i ~ /^B=/){ split($i,b,"="); bulls=b[2] }
    }
    if(nom!=""){
        tours[nom]++
        boeufs[nom]+=bulls
    }
}
$1=="FIN_PARTIE"{
    for(i=1;i<=NF;i++){
        if($i ~ /^NOM=/){ split($i,a,"="); gagnant=a[2] }
    }
}
END{
    print "=== STATS ==="
    for(n in tours){
        printf("%s: tours=%d boeufs=%d moyenne=%.2f\n", n, tours[n], boeufs[n], (tours[n]?boeufs[n]/tours[n]:0))
    }
    if(gagnant!="") print "GAGNANT:", gagnant
}
