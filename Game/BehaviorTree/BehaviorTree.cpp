// Behavior Tree Implementation - 5/24/23

#include "BehaviorTree.h"

void BehaviorTree::tick() {

	// TODO: traverse tree and update based on game state
	// 
	// start before updating?
	// this->m_Root.update();

	this->m_Root->tick();
}

