set terminal postscript landscape
set nolabel
set xlabel "Loss Rate (%)"
set xrange [0:10]
set ylabel "usec"
set yrange [0:25000000]
set output "udpa.ps"
plot "100mbpsa-1.dat" title "100mbps Stop-n-Wait" with linespoints, "100mbpsa-30.dat" title "100mbps Window=30" with linespoints, "1gbpsa-1.dat" title "1gbps Stop-n-Wait" with linespoints, "1gbpsa-30.dat" title "1gbps Window=30" with linespoints

