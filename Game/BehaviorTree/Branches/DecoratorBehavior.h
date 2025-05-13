// Represents a decorator node on the behavior tree

#pragma once

#include "../IBehavior.h"

// decorators are wrapper nodes with exactly one child that add nuance and 
// detail to the child logic
// instances to implement: repeat, hide return type, go one forever, etc... as needed

class DecoratorBehavior : public IBehavior {

protected:
	IBehavior* m_pChild;

public:
	DecoratorBehavior(IBehavior* i_child)
		: m_pChild(i_child)
	{

	}

	// TODO: add necessary IBehavior methods

};