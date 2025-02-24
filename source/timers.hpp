#ifndef _TIMERS_HPP_
#define _TIMERS_HPP_
#include <ogc/lwp_watchdog.h>
#include <cstddef>
#include <cstdint>
#include <ogc/video.h>
inline void time_reset() {
	settime(0);
}

inline uint64_t time_get() {
	return gettime();
}

inline int64_t time_diff_us(uint64_t f, uint64_t s) {
	return ticks_to_microsecs(diff_ticks(f, s));
}

inline int64_t time_diff_ms(uint64_t f, uint64_t s) {
	return ticks_to_millisecs(diff_ticks(f, s));
}

inline double time_diff_s(uint64_t f, uint64_t s) {
	return ticks_to_microsecs(diff_ticks(f, s)) / 1000000.0;
}

#endif