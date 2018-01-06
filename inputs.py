#! /usr/bin/python
# -*- coding: utf-8 -*-
"""
Python script to plot benchmarking results

Steps to produce and process results:
./bm_search --benchmark_out_format=csv --benchmark_out=search.csv
sed -i 's/\//,/g' search.csv
sed -i 's/ofs=//g' search.csv
"""

import matplotlib.pyplot as plt
import numpy as np

#driver,input,algo,offset,chars,iterations,real_time,cpu_time
data = np.genfromtxt('search.csv', delimiter=',', skip_header=9, 
                     usecols=(1,2,3,4,7), names=True, dtype="|S16,|S16,u1,u4,float")

#select only zero offsets
data0 = data[data['offset']==0]
regular = data0[data0['input'] == b'regular']
worst = data0[data0['input'] == b'worst']
best = data0[data0['input'] == b'best']
algos = list(set(data['algo']))

def algo_plot(inp, plot, title):
    plot.set_title(title, loc='left')
    #plot.set_xlabel('# of chars', horizontalalignment='right')
    plot.set_ylabel('CPU time, ns')
    for algo in algos:
        ds = inp[inp['algo'] == algo]
        plot.plot(ds['chars'], ds['cpu_time'], label=algo.decode('utf-8'))
        plot.grid(True)
        plot.set_xscale('log')
        plot.set_yscale('log')

fig = plt.figure(1)
algo_plot(regular, fig.add_subplot(311), 'Regular (offset=0)')
algo_plot(best, fig.add_subplot(312), 'Best (offset=0)')
algo_plot(worst, fig.add_subplot(313), 'Worst (offset=0)')
fig.legend()
fig.subplots_adjust(top=0.95,
                    bottom=0.05,
                    left=0.06,
                    right=0.88,
                    hspace=0.2,
                    wspace=0.2)

plt.show()

