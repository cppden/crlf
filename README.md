# crlf
Set of micro-benchmarks to compare searching for double CRLF.

Double CRLF is used to separate header and body in several 'human-readable' protocols like HTTP, SIP, etc.

The set includes common means provided by C and C++ RTL as well as few well known algos plus few hand-made dedicated routines for this only particular case.

The set is tested against different inputs: regular, worst and best, and multiple offsets where applicable.

std::strstr
std::string_view::find
std::string::find

Franek Jennings Smyth algorithm (which seems the fastest for our case among studied in https://arxiv.org/pdf/1012.2547v1.pdf)
Boyer Moore
Boyer Moore Horspool
