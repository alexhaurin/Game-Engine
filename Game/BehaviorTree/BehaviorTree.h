// Behavior Tree Structure - 5/24/23
// central point for storing and updating the behavior tree

#pragma once

#include "IBehavior.h"

class BehaviorTree {

protected:
	class IBehavior* m_Root; // tree root

public:

	BehaviorTree(class IBehavior* root)
		: m_Root(root)
	{
	}

	// traverses the tree from the root node and updates tree
	void tick();


};
