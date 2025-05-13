#pragma once

#include <chrono>
#include <cstdint>

namespace Mega
{
	using tTimestep = float; // used for update functions' delta time

	using tTime = double;
	using tNanosecond = std::chrono::nanoseconds;
	using tMicrosecond = std::chrono::microseconds;
	using tMillisecond = std::chrono::milliseconds;

	// Time since epoch in millies
	tTime Time();

	template<typename T>
	T Time()
	{
		return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now().time_since_epoch());
	}
}