// Implements an action behavior node of the behavior tree

#include <iostream>
#include "ActionBehavior.h"


BehaviorStatus ActionBehavior::tick() {

	// traverses node and children
	//  (m_eCurrentStatus != RUNNING) {
		// onInitialize();
	// } etc...

	/************************************************************************************
	Can check for each status in tick method and run onInitialize, update, and onTerminate,
	but this would require three functions to be passed into the constructor
	could instead just handle everything in the passed-in action function and change
	later if needed
	************************************************************************************/

	m_eCurrentStatus = m_action();

	return m_eCurrentStatus;

}

void ActionBehavior::onInitialize() {
	std::cout << "Initializing action behavior " << std::endl;

	// check certain conditions to see if action can be completed
	// if possible, run the update function
	// else, terminate with FAILURE status

}

BehaviorStatus ActionBehavior::update() {

	// do necessary checks
	// if possible, complete action
	// else, terminate with FAILURE status

	// when completed, terminate with SUCCESS status

	return SUCCESS;
}

void ActionBehavior::onTerminate(BehaviorStatus status) {

	// terminates action based on termination status

}