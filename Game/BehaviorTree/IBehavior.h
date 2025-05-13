// Behavior Structure - 5/24/23

#pragma once

enum BehaviorStatus {
	SUCCESS,
	FAILURE,
	RUNNING,
};

// behaviors are nodes of the behavior tree - leaf nodes contain actions and conditions,
// branches can be thought of as higher level behaviors
class IBehavior {

public:

	// traverses through this node's section of the tree
	virtual BehaviorStatus tick() = 0;

	// called once, immediately before first update call
	virtual void onInitialize() = 0;

	// called once each time the behavior tree updates
	virtual BehaviorStatus update() = 0; // returns a Status enum

	// called once, immediately after update() signals it's no longer running
	virtual void onTerminate(BehaviorStatus status) = 0;

};

