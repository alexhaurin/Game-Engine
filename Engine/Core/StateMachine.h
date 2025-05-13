#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <utility>

#include "Engine/Core/Time.h"
#include "Engine/Core/Math/Math.h"

#define SM_START_STATE 0
namespace Mega
{
	template<typename T>
	class StateMachine
	{
	public:
		using tStateIndex = int32_t;
		using tFunctionEntry = std::function<void(T*)>;
		using tFunctionUpdate = std::function<void(T*)>;
		using tFunctionExit = std::function<void(T*)>;

		struct State
		{
			tFunctionUpdate OnUpdate;
			tFunctionEntry OnEntry;
			tFunctionExit OnExit;
		};

		StateMachine() = default;
		StateMachine(T* in_classObject, const tStateIndex in_stateCount)
		{
			MEGA_ASSERT(in_classObject, "Null Class Object");

			m_classObject = in_classObject;
			m_states.resize(in_stateCount);
			m_stateTimers.resize(in_stateCount);
		}

		void Update(const tTimestep in_dt)
		{
			MEGA_ASSERT(m_classObject, "Updating with a null class object");
			MEGA_ASSERT(IsValid(m_currentState), "Updating Invalid State");

			// Update State Timers
			for (tStateIndex i = 0; i < m_stateTimers.size(); i++)
			{
				m_stateTimers[i] += in_dt;
			}

			UpdateTempState(m_temporaryState);

			// Manage State Transition
			const tFunctionUpdate& updateFunction = m_states[m_currentState].OnUpdate;
			if (updateFunction) { updateFunction(m_classObject); }
		}
		void AddState(tStateIndex in_state,
			tFunctionUpdate in_OnUpdate = nullptr,
			tFunctionEntry in_OnEntry = nullptr,
			tFunctionExit in_OnExit = nullptr
		)
		{
			MEGA_ASSERT(IsValid(in_state), "Invalid State Index");

			State newState;
			newState.OnEntry = in_OnEntry;
			newState.OnUpdate = in_OnUpdate;
			newState.OnExit = in_OnExit;

			m_states[in_state] = newState;
		}

		void SetState(const tStateIndex in_state)
		{
			m_temporaryState = in_state;
		}

		// Getters
		inline bool IsValid(tStateIndex in_state) const { return (in_state < m_states.size() && in_state >= 0); }

		inline tStateIndex GetState() const { return m_currentState; }
		inline tStateIndex GetLastState() const { return m_lastState; }
		inline tTimestep GetStateTimer(tStateIndex in_state) const
		{
			MEGA_ASSERT(IsValid(in_state), "Invalid State");
			return m_stateTimers[in_state];
		}

		inline bool IsState(const tStateIndex in_state) const { return m_currentState == in_state; }

	private:
		void UpdateTempState(tStateIndex in_tempState)
		{
			MEGA_ASSERT(m_classObject, "Setting state with null class object");
			MEGA_ASSERT(IsValid(in_tempState), "Invalid State Index");

			if (in_tempState == m_currentState) { return; }

			// Manage State Transition
			const tFunctionExit& exitFunction = m_states[m_currentState].OnExit;
			const tFunctionEntry& entryFunction = m_states[in_tempState].OnEntry;

			if (exitFunction) { exitFunction(m_classObject); }
			if (entryFunction) { entryFunction(m_classObject); }

			// Set State
			m_lastState = m_currentState;
			m_currentState = in_tempState;

			// Reset State Timers
			m_stateTimers[m_currentState] = 0;
			if (IsValid(m_lastState)) { m_stateTimers[m_lastState] = 0; }
		}

		T* m_classObject = nullptr;

		tStateIndex m_temporaryState = SM_START_STATE;
		tStateIndex m_currentState = SM_START_STATE;
		tStateIndex m_lastState = SM_START_STATE;

		std::vector<tTimestep> m_stateTimers = { 0 };
		std::vector<State> m_states;
	};
}