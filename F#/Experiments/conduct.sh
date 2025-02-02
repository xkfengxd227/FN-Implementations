if [ "$1" = "manual" ]; then
    dataset=$2
    queryset=$3
    qd=$4
    proj=$5
    pSeed=$6
    cuts=$7
else
    read -p "Dataset:" dataset
    read -p "Queryset:" queryset
    read -p "Query Dependent?" qd
    read -p "Projections:" proj
    read -p "pSeed:" pseed
    read -p "Cuts:" cuts
fi
fold=$dataset$Projections$qd

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    mkdir $fold
    mono ./release/Exeperiments.exe -expno 3 -data $dataset.txt -q $queryset.txt -proj $proj -cuts $cuts -qd $qd > ./$fold/result.txt
elif [[ "$OSTYPE" == "msys" ]]; then
    mkdir $fold
    ./release/Experiments.exe -data $dataset.txt -expno 3 -proj $proj -cuts $cuts -q $queryset.txt -qd $qd  > ./$fold/result.txt
fi
gnuplot <<EOF
set autoscale
set term png
set output "$fold/$dataset_$queryset_$fold.png"
set title "Approximation factor over cut. $dataset"
plot "./$fold/result.txt" using 1:2 title 'C avg' with linespoints
EOF

