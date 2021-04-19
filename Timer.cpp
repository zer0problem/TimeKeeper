#include "stdafx.h"
#include "Timer.h"

RM::Timer::Timer()
{
	myStartTime = std::chrono::high_resolution_clock::now();
	myLastTime = myStartTime;
	myDeltaTime = 0.0f;
	myTotalTime = 0.0;
}

void RM::Timer::Update()
{
	std::chrono::high_resolution_clock::time_point rightNow = std::chrono::high_resolution_clock::now();

	myDeltaTime = std::chrono::duration<float, std::ratio<1, 1>>(rightNow - myLastTime).count();
	myTotalTime = std::chrono::duration<double, std::ratio<1, 1>>(rightNow - myStartTime).count();

	myLastTime = rightNow;
}

float RM::Timer::GetDeltaTime() const
{
	return myDeltaTime;
}

double RM::Timer::GetTotalTime() const
{
	return myTotalTime;
}
