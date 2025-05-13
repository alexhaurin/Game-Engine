#pragma once

#include <cassert>
#include <stdexcept>

#define MEGA_WARNING_MSG(msg) std::cout << "ENGINE WARNING" << msg << std::endl;
#define MEGA_ERROR_MSG(msg) std::cerr << "ENGINE ERROR: " << msg << std::endl;

#define STRINGIZE(msg) #msg
#define MEGA_ASSERT(condition, msg) assert(condition && "MEGA ASSERT: " && msg)

#define MEGA_STATIC_ASSERT(...) static_assert(__VA_ARGS__)
#define MEGA_RUNTIME_ERROR(msg) throw std::runtime_error(std::string("MEGA RUNTIME ERROR: ") + msg)
// TODO: catch the error thrown here
#undef STRINGIZE

namespace Mega
{
	enum class eMegaResult
	{
		SUCCESS = 0,
		FAILURE = -1,
	};
}