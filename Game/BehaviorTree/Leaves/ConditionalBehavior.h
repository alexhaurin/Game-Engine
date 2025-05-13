// Represents a conditional leaf within the behavior tree

#pragma once

#include "../IBehavior.h"

class ConditionalBehavior : public IBehavior {

	// members to include:
	// ability to switch between InstantCheckMode and Monitoring mode
	// a negation option that checks for the opposite value than what is normally looked for

public:

	ConditionalBehavior()
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
