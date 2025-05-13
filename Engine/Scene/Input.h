#pragma once

namespace Mega
{
	struct Input
	{
		bool keyW = false;
		bool keyA = false;
		bool keyS = false;
		bool keyD = false;
		bool keyLShift = false;
		bool keySpace = false;
		bool keyTab = false;

		bool keyUp = false;
		bool keyDown = false;
		bool keyRight = false;
		bool keyLeft = false;

		double mousePosX = 0.0;
		double mousePosY = 0.0;
		double lastMousePosX = 0.0;
		double lastMousePosY = 0.0;
		double mouseMovementX = 0.0;
		double mouseMovementY = 0.0;
	};
}