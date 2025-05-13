// Represents the base class for composite behaviors (nodes capable of having multiple children) within the behavior tree

#pragma once

#include <vector>

#include "../IBehavior.h"

// this base class includes shared functionality for all composite nodes
// only specific child classes of this node should 
class CompositeBehavior : public IBehavior {

public:

	// no constructor

	void addChild(IBehavior* i_Child);
	void removeChild(IBehavior* i_Child);
	void clearChildren();

protected: 
	std::vector<IBehavior*> m_Children;

	// TODO: implement necessary IBehavior methods
	// virtual? 

};

