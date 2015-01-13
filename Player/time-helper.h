#ifndef _CXM_TIMER_HELPER_H_
#define _CXM_TIMER_HELPER_H_

#include <chrono>

namespace cxm {
namespace util {

class TimerHelper
{
	public: static int DayOfWeek(const std::chrono::system_clock::time_point &nowTime)
	{
		struct tm timeval;
		ToTimeVal(nowTime, &timeval);

		return timeval.tm_wday;
	}

	public: static int SecondsOfDay(const std::chrono::system_clock::time_point &nowTime)
	{
		struct tm timeval;
		ToTimeVal(nowTime, &timeval);
		return (timeval.tm_hour * 60 * 60 + timeval.tm_min * 60 + timeval.tm_sec);
	}

	public: static void ToTimeVal(const std::chrono::system_clock::time_point &nowTime,  struct tm *ptimeval)
	{
#ifdef _WIN32
		time_t tt = std::chrono::system_clock::to_time_t(nowTime);
		localtime_s(ptimeval, &tt);
#else
#endif
	}
};

}
}
#endif
