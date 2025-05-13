#pragma once

#include <bitset>
#include <cstdint>
#include <iostream>

namespace Mega
{
	struct StateController
	{
	public:
		using tState = uint32_t;

		inline tState GetSingleState() const { return m_singleState; }
		// Sets the single state
		inline void SetState(const tState in_state) { m_singleState = in_state; } //(0b1 << in_state); }
		// Adds the given state to the multiple state variable
		inline void AddState(const tState in_state) { m_multipleState |= (0b1 << in_state); }
		// Removes the given state from the multiple state variable
		inline void RemoveState(const tState in_state) { m_multipleState &= ~(0b1 << in_state); }

		// Returns true/false based on if the given state is set or not
		inline bool Is(const tState in_state) const { return (m_singleState == in_state) || (m_multipleState & (0b1 << in_state)); }

		inline void Print()
		{
			std::cout << "Single State: " << (m_singleState) << '\n';
			std::cout << "Multiple State: " << std::bitset<sizeof(tState) * 8>(m_multipleState) << std::endl;
		}

	private:
		tState m_multipleState = 0;
		tState m_singleState = 0;
	};
}