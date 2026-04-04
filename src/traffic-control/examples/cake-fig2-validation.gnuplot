set terminal pngcairo size 1000,500 enhanced font 'Sans,12'
set output 'cake-fig2-validation.png'

set title 'Figure 2: Latency under Load — CAKE vs. DropTail Baseline'
set xlabel 'Time (s)'
set ylabel 'One-way Latency RTT/2 (ms)'

set grid
set key top right box opaque
set rmargin 15

set yrange [0:75]
set ytics 0,10,75

set arrow from graph 0,first 7  to graph 1,first 7  nohead lc rgb '#888888' lw 1.5 dt 2
set arrow from graph 0,first 45 to graph 1,first 45 nohead lc rgb '#888888' lw 1.5 dt 2
set arrow from graph 0,first 62 to graph 1,first 62 nohead lc rgb '#888888' lw 1.5 dt 2

set label '7 ms (Floor)' at graph 1.02, first 7  font 'Sans,10' tc rgb '#555555'
set label '45 ms (Min)' at graph 1.02, first 45 font 'Sans,10' tc rgb '#555555'
set label '62 ms (Peak)' at graph 1.02, first 62 font 'Sans,10' tc rgb '#555555'

plot \
  'cake-fig2-noaqm.dat' using 1:2 with lines lw 1.2 lc rgb '#ff7f0e' title 'Baseline (DropTail 180p)', \
  'cake-fig2-cake.dat' using 1:2 with lines lw 2.2 lc rgb '#1f77b4' title 'CAKE (Shaper + COBALT)'
