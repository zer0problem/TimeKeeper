#pragma once
#include <chrono>

namespace RM
{
	class Timer
	{
	public:
		Timer();
		Timer(const Timer &other) = delete;
		Timer &operator= (const Timer &other) = delete;

		void Update();

		float GetDeltaTime() const;
		double GetTotalTime() const;

	private:
		std::chrono::high_resolution_clock::time_point myStartTime;
		std::chrono::high_resolution_clock::time_point myLastTime;
		float myDeltaTime;
		double myTotalTime;
	};
}
