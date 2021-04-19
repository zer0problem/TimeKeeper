#pragma once

namespace  RM
{
	namespace TimeKeeper
	{
		// key, is the marking it will receive in the list
		void Start(const char* aKey);
		void Stop();
		
		// if no thread name is given, uses "Thread:" + thread id
		void StartFrame(const char* aThreadName = nullptr);
		void StopFrame();

		// if no window name is given, it expects a window to be created already
		void Display(const char* aWindowName = nullptr);
		
		void Reset();
	}
}
