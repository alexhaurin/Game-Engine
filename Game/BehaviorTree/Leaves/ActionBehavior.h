// Represents an action behavior node within the behavior tree

#pragma once

#include <functional>

#include "../IBehavior.h"

class ActionBehavior : public IBehavior {

protected:
	BehaviorStatus m_eCurrentStatus;

private:
	using action_t = std::function<BehaviorStatus()>;
	action_t m_action;

	bool canHaveChildren;

public:

	ActionBehavior(action_t i_action)
		: m_eCurrentStatus(BehaviorStatus::RUNNING), m_action(i_action), canHaveChildren(false)
	{

	}

	// traverses through this node's section of the tree
	BehaviorStatus tick();

	// called once, immediately before first update call
	void onInitialize();

	// called once each time the behavior tree updates
	BehaviorStatus update(); // returns a Status enum

	// called once, immediately after update() signals it's no longer running
	void onTerminate(BehaviorStatus status);


};