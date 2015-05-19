set terminal postscript landscape
set key on left
set nolabel
set xlabel "window"
set xrange [0:30]
set ylabel "usec"
set yrange [0:38000000]
set output "udp.ps"
plot "100mbps_sw.dat" title "100mbps sliding window" with linespoints, "1gbps_sw.dat" title "1gbps sliding window" with linespoints, 30860033 title "100mbs stopNwait" with line, 4813490 title "1gbps stopNwait" with line
