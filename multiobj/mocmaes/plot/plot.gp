set term x11 persist
set xlabel 'Obj #1'
set ylabel 'Obj #2'
plot 'pareto_mocma.txt' u 2:3
