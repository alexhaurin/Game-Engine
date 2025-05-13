#include "Time.h"

namespace Mega
{
	tTime Time()
	{
		//return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return std::chrono::duration_cast<Mega::tMillisecond>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	}
} // namespace Mega