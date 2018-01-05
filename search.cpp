#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>
#include <functional>

#include <benchmark/benchmark.h>

/*
 * Overall study: https://arxiv.org/pdf/1012.2547v1.pdf
 */

//Franek Jennings Smyth algorithm
//https://github.com/CGJennings/fjs-string-matching
namespace fjs {

template <std::size_t N>
class searcher
{
public:
	explicit searcher(char const* needle)
	{
		for (std::size_t i = 0; i < N; ++i)
		{
			m_needle[i] = needle[i];
		}

		//makebetap
		int j = m_betap[0] = -1;
		for (std::size_t i = 0; i < N; )
		{
			while (j > -1 && m_needle[i] != m_needle[j])
			{
				j = m_betap[j];
			}
			i++;
			j++;
			m_betap[i] = (i < N && m_needle[i] == m_needle[j]) ? m_betap[j] : j;
			//printf("betap[%zu]=%d\n", i, m_betap[i]);
		}

		//makeDelta
		std::memset(m_delta, N + 1, sizeof(m_delta));
		for (std::size_t i = 0; i < N; ++i)
		{
			m_delta[ std::size_t(m_needle[i]) ] = N - i;
		}
		//for (auto c : m_delta) printf("%02u ", c);
	}

	std::size_t operator() (std::string_view const& hay) const { return find(hay); }
	std::size_t find(std::string_view const& hay) const
	{
		auto const needle_back = m_needle[N - 1];
		auto const hay_size = hay.size();
		int8_t j = 0;
		for (std::size_t i = 0, ip = N - 1; ip < hay_size; ip = (N - 1) + i - j)
		{
			if (j > 0) //check whole needle
			{
				for ( ; std::size_t(j) < N && hay[i] == m_needle[std::size_t(j)]; ++i, ++j) {}
				if (std::size_t(j) == N) return i - N;
				j = m_betap[j];
			}
			else //find matching back char of needle
			{
				for ( ; hay[ip] != needle_back; ip += m_delta[ std::size_t(hay[ip + 1]) ])
				{
					if (ip >= hay_size) return std::string_view::npos;
				}

				for (j = 0, i = ip - (N - 1); j < int8_t(N - 1) && hay[i] == m_needle[j]; ++i, ++j) {}

				if (j == 0) { ++i; }
				else if (j == int(N - 1)) { return i - (N - 1); }
				else { j = m_betap[j]; }
			}
		}

		//printf("last\n");
		return std::string_view::npos;
	}

private:
	char    m_needle[N];
	uint8_t m_delta[256];
	int8_t  m_betap[N+1];
};

} //end: fjs

#ifdef __LITTLE_ENDIAN
constexpr inline uint16_t two_chars(char one, char two) { return (uint16_t(two) << 8) | one; }
constexpr inline char prev_char(uint16_t tc)            { return tc >> 8; }
constexpr inline char next_char(uint16_t tc)            { return char(tc); }

constexpr inline uint32_t three_chars(char one, char two, char three)
{
	return (uint32_t(three) << 16) | two_chars(one, two);
}
constexpr inline uint32_t four_chars(char one, char two, char three, char four)
{
	return (uint32_t(four) << 24) | three_chars(one, two, three);
}
constexpr inline uint32_t four_chars(uint8_t one)       { return four_chars(one, one, one, one); }
constexpr inline uint32_t four_chars(char one, char two){ return four_chars(one, two, one, two); }
constexpr inline char four_chars_1(uint32_t fc)         { return char(fc); }
constexpr inline char four_chars_3(uint32_t fc)         { return char(fc >> 16); }
constexpr inline char four_chars_4(uint32_t fc)         { return fc >> 24; }
constexpr inline uint16_t four_chars_12(uint32_t fc)    { return uint16_t(fc); }
constexpr inline uint16_t four_chars_23(uint32_t fc)    { return uint16_t(fc >> 8); }
constexpr inline uint32_t four_chars_123(uint32_t fc)   { return fc & 0xFFFFFF; }
#else
constexpr inline uint16_t two_chars(char one, char two) { return (uint16_t(one) << 8) | two; }
constexpr inline char prev_char(uint16_t tc)            { return char(tc); }
constexpr inline char next_char(uint16_t tc)            { return tc >> 8; }
#endif

//reading by words
char const* dcrlf(std::string_view const& hay)
{
	uint16_t const* pw = reinterpret_cast<uint16_t const*>(hay.data() + (std::size_t(hay.data()) & 1));
	uint16_t const* pe = pw + 1 + hay.size() / 2;
	for (; pw < pe; ++pw)
	{
		switch (*pw)
		{
		case two_chars('\r','\n'): // "\r\n" found, check next is "\r\n"
			if (two_chars('\r','\n') == pw[1]) return reinterpret_cast<char const*>(pw);
			//printf("crlf|%02X|%02X\n", reinterpret_cast<char const*>(pw+1)[0], reinterpret_cast<char const*>(pw+1)[1]);
			break;

		case two_chars('\n','\r'): // "\n\r" found, check prev "\r" and next "\n"
			if ('\r' == reinterpret_cast<char const*>(pw)[-1]
			&& '\n' == reinterpret_cast<char const*>(pw + 1)[0])
				return reinterpret_cast<char const*>(pw) - 1;
			//printf("%02X|lfcr|%02X\n", reinterpret_cast<char const*>(pw)[-1], reinterpret_cast<char const*>(pw+1)[0]);
			break;

		default:
			break;
		};
	}
	return nullptr;
}

//reading by words with skip
char const* dcrlf2(std::string_view const& hay)
{
	uint16_t const* pw = reinterpret_cast<uint16_t const*>(hay.data() + (std::size_t(hay.data()) & 1));
	uint16_t const* pe = pw + 1 + hay.size() / 2;
	for (; pw < pe; ++pw)
	{
		switch (*pw)
		{
		case two_chars('\r','\n'): // "\r\n" found, check next is "\r\n"
			if (two_chars('\r','\n') == pw[1]) return reinterpret_cast<char const*>(pw);
			++pw;
			break;

		case two_chars('\n','\r'): // "\n\r" found, check prev "\r" and next "\n"
			if ('\n' == reinterpret_cast<char const*>(pw + 1)[0])
			{
				if ('\r' == reinterpret_cast<char const*>(pw)[-1]) return reinterpret_cast<char const*>(pw) - 1;
				++pw;
			}
			break;

		default:
			break;
		};
	}
	return nullptr;
}

//reading by words with skip and saving previous
char const* dcrlf3(std::string_view const& hay)
{
	uint16_t const* pw = reinterpret_cast<uint16_t const*>(hay.data() + (std::size_t(hay.data()) & 1));
	uint16_t const* pe = pw + 1 + hay.size() / 2;
	uint16_t prev = 0;
	for (; pw < pe; ++pw)
	{
		switch (uint16_t curr = *pw)
		{
		case two_chars('\r','\n'): // "\r\n" found, check next is "\r\n"
			prev = *++pw;
			if (two_chars('\r','\n') == prev) return reinterpret_cast<char const*>(pw - 1);
			break;

		case two_chars('\n','\r'): // "\n\r" found, check prev "\r" and next "\n"
			curr = *++pw;
			if ('\r' == prev_char(prev) && '\n' == next_char(curr))
			{
				return reinterpret_cast<char const*>(pw - 1) - 1;
			}
			[[fallthrough]];

		default:
			prev = curr;
			break;
		};
	}
	return nullptr;
}

namespace quad {

enum class state
{
	XXXX,
	XRNR,
	XXRN,
	XXXR,
};

inline state init_state(char const* psz)
{
	uint32_t chars = 0;
	switch ((std::size_t(psz) & 0b11))
	{
	case 3:
		chars = (chars << 8) | *psz++;
		[[fallthrough]];
	case 2:
		chars = (chars << 8) | *psz++;
		[[fallthrough]];
	case 1:
		chars = (chars << 8) | *psz++;
		[[fallthrough]];
	default:
		switch (chars)
		{
		case 0x000D0A0D: return state::XRNR;
		case 0x00000D0A: return state::XXRN;
		case 0x0000000D: return state::XXXR;
		default:         return state::XXXX;
		}
	}
}

inline state next_state(uint32_t curr)
{
	switch (four_chars_4(curr))
	{
	case '\r': return (two_chars('\r','\n') == four_chars_23(curr)) ? state::XRNR : state::XXXR;
	case '\n': return ('\r' == four_chars_3(curr)) ? state::XXRN : state::XXXX;
	default:   return state::XXXX;
	}
}

//reading by dwords with skip and saving previous
char const* dcrlf(std::string_view const& hay)
{
	uint32_t const* pw = reinterpret_cast<uint32_t const*>(hay.data() + (std::size_t(hay.data()) & 0b11));
	auto const* pe = pw + 3 + hay.size() / 4;
	state st = init_state(hay.data());

	for (; pw < pe; ++pw)
	{
		auto const curr = *pw;
		switch (st)
		{
		case state::XXXX:
			if (four_chars('\r','\n') == curr) return reinterpret_cast<char const*>(pw);
			break;
		case state::XRNR:
			if ('\n' == four_chars_1(curr)) return reinterpret_cast<char const*>(pw) - 3;
			break;
		case state::XXRN:
			if (two_chars('\r','\n') == four_chars_12(curr)) return reinterpret_cast<char const*>(pw) - 2;
			break;
		case state::XXXR:
			if (three_chars('\n','\r','\n') == four_chars_123(curr)) return reinterpret_cast<char const*>(pw) - 1;
			break;
		default: //can't happen
			abort();
			break;
		}
		st = next_state(curr);
	}
	return nullptr;
}

} //end: namespace quad


using namespace std::literals;

//3 different inputs - worst (rrrn...rrrnrn), regular, best (...rnrn)
//regular input haystack: several lines terminated with "\r\n"
std::string input_regular(std::size_t size)
{
	std::string out;
	out.reserve(size + 7);

	std::size_t line = 10;
	for (char c = ' '; out.size() < size; ++c)
	{
		if (c > '~') c = ' ';
		out += c;

		if (0 == (out.size() % line))
		{
			out += "\r\n";
			line++;
		}
	}

	if (out.back() == '\n') out += "\r\n";
	else out += "\r\n\r\n";

	return out;
}

//worst case input haystack: rrrn...rrrnrn
std::string input_worst(std::size_t size)
{
	std::string out;
	out.reserve(size + 7);
	while (out.size() < size)
	{
		out += "\r\r\r\n";
	}

	out += "\n\r\n\r\n";

	return out;
}

//best case input haystack: ...rnrn
std::string input_best(std::size_t size)
{
	std::string out(size, '.');
	out += "\r\n\r\n";
	return out;
}


template <class HAYGEN, class FUNC>
void BM_crlf(benchmark::State& state, HAYGEN&& make_haystack, std::size_t OFFSET, FUNC&& func)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	std::string_view const hay_sv = std::string_view(hay).substr(OFFSET);
	auto const pos = func(hay_sv);
	if (std::size_t(pos - hay_sv.begin()) != (hay_sv.size() - 4))
	{
		std::printf("Invalid pos = %p\n", pos);
		abort();
	}

	for (auto _ : state)
	{
		auto const res = func(hay_sv);
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_strstr(benchmark::State& state, HAYGEN&& make_haystack, std::size_t OFFSET)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	std::string const needle{"\r\n\r\n"};
	char const* const psz = hay.c_str() + OFFSET;
	auto const pos = std::strstr(psz, needle.c_str());
	if (std::size_t(pos - psz) != (hay.size() - OFFSET - 4))
	{
		std::printf("Invalid pos\n");
		abort();
	}

	for (auto _ : state)
	{
		auto const res = std::strstr(psz, needle.c_str());
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_string(benchmark::State& state, HAYGEN&& make_haystack)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	std::string const needle{"\r\n\r\n"};
	auto const pos = hay.find(needle);
	if (pos != (hay.size() - 4))
	{
		std::printf("Invalid pos=%zu\n", pos);
		abort();
	}

	for (auto _ : state)
	{
		auto const res = hay.find(needle);
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_sview(benchmark::State& state, HAYGEN&& make_haystack, std::size_t OFFSET)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	auto const needle_sv = "\r\n\r\n"sv;
	std::string_view const hay_sv = std::string_view(hay).substr(OFFSET);
	auto const pos = hay_sv.find(needle_sv);
	if (pos != (hay_sv.size() - 4))
	{
		std::printf("Invalid pos=%zu\n", pos);
		abort();
	}

	for (auto _ : state)
	{
		auto const res = hay_sv.find(needle_sv);
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_bm(benchmark::State& state, HAYGEN&& make_haystack)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	auto const needle_sv = "\r\n\r\n"sv;
	std::string_view const hay_sv = hay;

	std::boyer_moore_searcher const searcher{needle_sv.begin(), needle_sv.end()};

	auto const pit = searcher(hay_sv.begin(), hay_sv.end());
	if (pit.first == pit.second || (hay.size() - 4) != std::size_t(pit.first - hay_sv.begin()))
	{
		std::printf("Invalid search\n");
		abort();
	}

	for (auto _ : state)
	{
		auto const res = searcher(hay_sv.begin(), hay_sv.end());
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_bmh(benchmark::State& state, HAYGEN&& make_haystack)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	auto const needle_sv = "\r\n\r\n"sv;
	std::string_view const hay_sv = hay;

	std::boyer_moore_horspool_searcher const searcher{needle_sv.begin(), needle_sv.end()};

	auto const pit = searcher(hay_sv.begin(), hay_sv.end());
	if (pit.first == pit.second || (hay.size() - 4) != std::size_t(pit.first - hay_sv.begin()))
	{
		std::printf("Invalid search\n");
		abort();
	}

	for (auto _ : state)
	{
		auto const res = searcher(hay_sv.begin(), hay_sv.end());
		benchmark::DoNotOptimize(res);
	}
}

template <class HAYGEN>
void BM_fjs(benchmark::State& state, HAYGEN&& make_haystack)
{
	std::string const hay = make_haystack(state.range());
	state.SetBytesProcessed(hay.size());
	std::string_view const hay_sv = hay;
	fjs::searcher<4> fs{"\r\n\r\n"};
	auto const pos = fs(hay_sv);
	if (pos != (hay_sv.size() - 4))
	{
		std::printf("Invalid pos = %zu\n", pos);
		abort();
	}

	for (auto _ : state)
	{
		auto const res = fs(hay_sv);
		benchmark::DoNotOptimize(res);
	}
}


//constexpr std::size_t UPTO = 256;
//constexpr std::size_t UPTO = 1024;
constexpr std::size_t UPTO = 64*1024;

#undef INPUT
#define INPUT input_regular
BENCHMARK_CAPTURE(BM_crlf,   regular/d1/ofs=0,     INPUT, 0, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/d1/ofs=1,     INPUT, 1, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/d2/ofs=0,     INPUT, 0, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/d2/ofs=1,     INPUT, 1, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/d3/ofs=0,     INPUT, 0, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/d3/ofs=1,     INPUT, 1, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/qd/ofs=0,     INPUT, 0, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/qd/ofs=1,     INPUT, 1, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/qd/ofs=2,     INPUT, 2, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   regular/qd/ofs=3,     INPUT, 3, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, regular/strstr/ofs=0, INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, regular/strstr/ofs=1, INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_fjs,    regular/fjs,          INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  regular/sview/ofs=0,  INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  regular/sview/ofs=1,  INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_string, regular/string,       INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bm,     regular/boyer_moore,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bmh,    regular/bm_horspool,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);

#undef INPUT
#define INPUT input_worst
BENCHMARK_CAPTURE(BM_crlf,   worst/d1/ofs=0,     INPUT, 0, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/d1/ofs=1,     INPUT, 1, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/d2/ofs=0,     INPUT, 0, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/d2/ofs=1,     INPUT, 1, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/d3/ofs=0,     INPUT, 0, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/d3/ofs=1,     INPUT, 1, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/qd/ofs=0,     INPUT, 0, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/qd/ofs=1,     INPUT, 1, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/qd/ofs=2,     INPUT, 2, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   worst/qd/ofs=3,     INPUT, 3, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, worst/strstr/ofs=0, INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, worst/strstr/ofs=1, INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_fjs,    worst/fjs,          INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  worst/sview/ofs=0,  INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  worst/sview/ofs=1,  INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_string, worst/string,       INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bm,     worst/boyer_moore,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bmh,    worst/bm_horspool,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);

#undef INPUT
#define INPUT input_best
BENCHMARK_CAPTURE(BM_crlf,   best/d1/ofs=0,     INPUT, 0, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/d1/ofs=1,     INPUT, 1, dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/d2/ofs=0,     INPUT, 0, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/d2/ofs=1,     INPUT, 1, dcrlf2)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/d3/ofs=0,     INPUT, 0, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/d3/ofs=1,     INPUT, 1, dcrlf3)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/qd/ofs=0,     INPUT, 0, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/qd/ofs=1,     INPUT, 1, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/qd/ofs=2,     INPUT, 2, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_crlf,   best/qd/ofs=3,     INPUT, 3, quad::dcrlf)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, best/strstr/ofs=0, INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_strstr, best/strstr/ofs=1, INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_fjs,    best/fjs,          INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  best/sview/ofs=0,  INPUT, 0)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_sview,  best/sview/ofs=1,  INPUT, 1)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_string, best/string,       INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bm,     best/boyer_moore,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);
BENCHMARK_CAPTURE(BM_bmh,    best/bm_horspool,  INPUT)->RangeMultiplier(2)->Range(16, UPTO);


BENCHMARK_MAIN();
