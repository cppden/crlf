CRLF
====
## Set of micro-benchmarks to compare searching for double CRLF.

Double CRLF is used to separate header and body in several 'human-readable' protocols like HTTP, SIP, etc.

The set includes common means provided by C and C++ RTL as well as few well known algos plus few hand-made dedicated routines for this only particular case.

The set is tested against different inputs: regular, worst and best, and multiple offsets where applicable.

### List of searchers
1. std::strstr
* std::string_view::find
* std::string::find
* Franek Jennings Smyth algorithm
* Boyer Moore
* Boyer Moore Horspool
* d1-searcher: reads by words (two bytes at once)
* d2-searcher: reads by words skipping next when possible, looks back when needed
* d3-searcher: reads by words with skips and saving previous to avoid look-back
* qd-searcher: reads by double words with skips and saving previous

### Notes
* FJS algorithm was chosen as the fastest for our case among studied in https://arxiv.org/pdf/1012.2547v1.pdf.
* Boyer Moore algos are now included in STL.
* Custom algos currently do not care of reading beyond the input since were made only for comparison. Though adding proper handling of end of input shouldn't cause noticeable degradation.

# Results and conclusions
![chart](https://raw.githubusercontent.com/cppden/crlf/master/img/inputs.png)

### Notes
* The both axes have logarithmic scale.
* Results are obtained for gcc7.2 with -O3 running on i7-3517U CPU at 3 GHz.


1. Regular input
  * The best performing searchers are custom dedicated routines d2 and qd.
  * The worst performing searcher is std::string_view::find and std::string::find is next.
  * std::strstr outperforms even FJS.
  * As expected FJS outperforms both Boyer-Moore and Boyer-Moore-Horspool.

2. Best input
  * The best performing searcher is std::string::find which is due to recent optimization in STL for this particular function (uses strchr to find 1st char of pattern).
  * The next to it is std::strstr.
  * The worst performing searchers are Boyer-Moore and Boyer-Moore-Horspool.

3. Worst input
  * The best is d2 with qd next.
  * The worst is std::string::find which is again due to the recent optimization.

4. Alignment of input data.
  * There were no noticeable impact on input alignment observed.
