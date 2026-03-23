# Usage:
#   ./ns3 run cake-validation
#   gnuplot src/traffic-control/examples/cake-validation.gnuplot
set terminal pngcairo size 800,600 enhanced font 'Verdana,10'
set output 'cake-validation.png'

set title "Figure 3 Reproduction: CAKE Multi-Flow Fairness (10 Mbps Bottleneck)"
set xlabel "Time (seconds)"
set ylabel "Throughput (Mbps)"
set grid
set key outside

set xrange [0:62]
set yrange [0:11]

set arrow from 10,0 to 10,11 nohead dt 2 lc rgb "gray"
set arrow from 20,0 to 20,11 nohead dt 2 lc rgb "gray"

plot "cake-validation.dat" using 1:2 with lines lw 2 title "Flow 1 (0s)", \
     "" using 1:3 with lines lw 2 title "Flow 2 (10s)", \
     "" using 1:4 with lines lw 2 title "Flow 3 (20s)"
