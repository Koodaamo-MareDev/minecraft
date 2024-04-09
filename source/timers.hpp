#ifndef _TIMERS_HPP_
#define _TIMERS_HPP_
#include <ogc/lwp_watchdog.h>
#include <cstddef>
inline void time_reset() {
	settime(0);
}

inline uint64_t time_get() {
	return gettime();
}

inline int64_t time_diff_us(uint64_t f, uint64_t s) {
	return (s - f) / (TB_TIMER_CLOCK / 1000UL);
}

inline int64_t time_diff_ms(uint64_t f, uint64_t s) {
	return (s - f) / TB_TIMER_CLOCK;
}

inline float time_diff_s(uint64_t f, uint64_t s) {
	return float(double(s - f) / double(1000UL * TB_TIMER_CLOCK));
}

#endif