#include "stdafx.h"
#include "TimeKeeper.h"
#include <map> // used to store relations
#include <thread> // used for thread::id
#include <mutex> // used to lock the queue
#include <queue> // used to store all frame data before parsing
#include "Timer.h"

// Change this to your main ImGui include file
#include "imgui\imgui.h"

namespace RM
{
	namespace TimeKeeper
	{
		struct SRecord
		{
			std::string myName;
			unsigned int myFrameCount = 0;
			double myTime;
			std::map<std::string, SRecord> myChildren;
		};

		struct STable
		{
			std::string myName;
			double myTime = 0.0f;
			std::chrono::time_point<std::chrono::high_resolution_clock> myStartTime;
			STable* myParent = nullptr;
			std::vector<STable> myChildren;
		};

		struct SFrameInfo
		{
			std::thread::id myThreadId;
			STable myRoot;
			STable* myCurrentTable = nullptr;
		};

		static thread_local SFrameInfo threadFrameInfo;

		static std::mutex globalFrameQueueMutex;
		static std::queue<SFrameInfo> globalFrameQueue;
		static std::map<std::thread::id, SRecord> globalThreadRecords;
		static bool globalDisplayPercent = true;
		static bool globalDisplayFrameTime = true;
		static bool globalDisplayEntry = true;

		void StartFrame(const char* aThreadName)
		{
			threadFrameInfo.myRoot.myChildren.clear();
			threadFrameInfo.myRoot.myStartTime = std::chrono::high_resolution_clock::now();
			if (aThreadName != nullptr)
			{
				threadFrameInfo.myRoot.myName = aThreadName;
			}
			else
			{
				auto id = std::this_thread::get_id();
				std::stringstream ss;
				ss << "Thread: " << id;
				threadFrameInfo.myRoot.myName = ss.str();
			}
			threadFrameInfo.myThreadId = std::this_thread::get_id();
			threadFrameInfo.myCurrentTable = &threadFrameInfo.myRoot;
		}

		void StopFrame()
		{
			auto endTime = std::chrono::high_resolution_clock::now();
			threadFrameInfo.myRoot.myTime = std::chrono::duration<double>(endTime - threadFrameInfo.myCurrentTable->myStartTime).count();
			std::unique_lock<std::mutex> lock(globalFrameQueueMutex);
			globalFrameQueue.push(threadFrameInfo);
		}

		void Start(const char* aKey)
		{
			STable& curr = *threadFrameInfo.myCurrentTable;
			curr.myChildren.push_back({ aKey, 0.0f, std::chrono::high_resolution_clock::now(), &curr, {} });
			threadFrameInfo.myCurrentTable = &curr.myChildren.back();
		}

		void Stop()
		{
			auto endTime = std::chrono::high_resolution_clock::now();
			threadFrameInfo.myCurrentTable->myTime = std::chrono::duration<double>(endTime - threadFrameInfo.myCurrentTable->myStartTime).count();
			threadFrameInfo.myCurrentTable = threadFrameInfo.myCurrentTable->myParent;
		}

		void Reset()
		{
			for (auto& [key, value] : globalThreadRecords)
			{
				value.myFrameCount = 0;
				value.myTime = 0.0f;
				value.myChildren.clear();
			}
		}

		float DisplayStats(float aPercent, float aFrameTime, float aEntryPercent)
		{
			float indent = 0.0f;
			if (globalDisplayPercent)
			{
				indent += 150.0f;
				ImGui::SameLine();
				ImGui::Indent(150);
				ImGui::Text("%.2f%%", aPercent);
			}
			if (globalDisplayFrameTime)
			{
				indent += 150.0f;
				ImGui::SameLine();
				ImGui::Indent(150);
				ImGui::Text("%.4f\ms", aFrameTime);
			}
			if (globalDisplayEntry)
			{
				indent += 150.0f;
				ImGui::SameLine();
				ImGui::Indent(150);
				ImGui::Text("%.2f Entries/Frame", aEntryPercent);
			}
			return indent;
		}

		void RecursiveDisplay(const SRecord& aTable, unsigned int aTotalFrameCount, float aTotalTime)
		{
			float entriesPerFrame = static_cast<float>(aTable.myFrameCount) / static_cast<float>(aTotalFrameCount);
			float percent = aTable.myTime / aTotalTime * 100.0f;
			float frameTimeMs = entriesPerFrame * aTable.myTime / static_cast<double>(aTable.myFrameCount) * 1000.0f;

			if (aTable.myChildren.size() > 0)
			{
				if (ImGui::TreeNode(aTable.myName.c_str()))
				{
					ImGui::Unindent(DisplayStats(percent, frameTimeMs, entriesPerFrame));
					for (auto& [key, value] : aTable.myChildren)
					{
						RecursiveDisplay(value, aTotalFrameCount, aTotalTime);
					}
					ImGui::TreePop();
				}
				else
				{
					ImGui::Indent(21);
					ImGui::Unindent(DisplayStats(percent, frameTimeMs, entriesPerFrame));
					ImGui::Unindent(21);
				}
			}
			else
			{
				ImGui::Indent(21);
				ImGui::TextUnformatted(aTable.myName.c_str());
				ImGui::Unindent(DisplayStats(percent, frameTimeMs, entriesPerFrame));
				ImGui::Unindent(21);
			}
		}

		void RecursiveFill(SRecord& aRecord, const STable& aTable)
		{
			aRecord.myName = aTable.myName;
			aRecord.myFrameCount++;
			aRecord.myTime += aTable.myTime;
			for (const STable& subTable : aTable.myChildren)
			{
				RecursiveFill(aRecord.myChildren[subTable.myName], subTable);
			}
		}

		void Display(const char* aWindowName)
		{
			while (globalFrameQueue.size() > 0)
			{
				SFrameInfo frameInfo;
				{
					std::unique_lock<std::mutex> lock(globalFrameQueueMutex);
					frameInfo = globalFrameQueue.front();
					globalFrameQueue.pop();
				}

				SRecord& record = globalThreadRecords[frameInfo.myThreadId];

				RecursiveFill(record, frameInfo.myRoot);
			}

			static Timer t = Timer();
			static constexpr int maxFrameTimes = 300;
			static constexpr int halfMaxFrameTimes = maxFrameTimes / 2;
			static float frameTimes[maxFrameTimes];
			static unsigned int currFrame = 0u;
			static float maxDisplayFPS = 144.0f;
			static float minDisplayFPS = 10.0f;
			t.Update();
			frameTimes[currFrame] = t.GetDeltaTime();
			currFrame = (currFrame + 1) % 300;

			float framesPerSecond[maxFrameTimes];
			float minFPS = FLT_MAX;
			float maxFPS = FLT_MIN;
			float averageFPS = 0;
			float minFrameTime = FLT_MAX;
			float maxFrameTime = FLT_MIN;
			float averageFrameTime = 0;
			for (size_t i = 0; i < maxFrameTimes; ++i)
			{
				framesPerSecond[i] = 1.0f / frameTimes[i];
				averageFPS += framesPerSecond[i] / static_cast<float>(maxFrameTimes);
				minFPS = std::min(framesPerSecond[i], minFPS);
				maxFPS = std::max(framesPerSecond[i], maxFPS);

				averageFrameTime += frameTimes[i] / static_cast<float>(maxFrameTimes);
				minFrameTime = std::min(frameTimes[i], minFrameTime);
				maxFrameTime = std::max(frameTimes[i], maxFrameTime);
			}

			if (aWindowName != nullptr)
			{
				ImGui::Begin(aWindowName);
			}
			if (ImGui::TreeNode("Performance Stats"))
			{
				for (const auto& threadIt : globalThreadRecords)
				{
					RecursiveDisplay(threadIt.second, threadIt.second.myFrameCount, threadIt.second.myTime);
				}
				ImGui::Checkbox("Show \%", &globalDisplayPercent);
				ImGui::Checkbox("Show ms", &globalDisplayFrameTime);
				ImGui::Checkbox("Show entries/frame", &globalDisplayEntry);
				if (ImGui::Button("Reset"))
				{
					Reset();
				}
				ImGui::DragFloat("Min FPS Range", &minDisplayFPS, 1.0f, 1.0f, 1000.0f);
				ImGui::DragFloat("Max FPS Range", &maxDisplayFPS, 1.0f, 1.0f, 1000.0f);
				ImGui::Text("Min FPS: %f", minFPS);
				ImGui::Text("Max FPS: %f", maxFPS);
				ImGui::Text("Avg FPS: %f", averageFPS);
				ImGui::PlotLines("FPS", framesPerSecond, halfMaxFrameTimes, currFrame % halfMaxFrameTimes, nullptr, minDisplayFPS, maxDisplayFPS, ImVec2(600, 90.0f));
				ImGui::Text("Min FrameTime: %f", minFrameTime);
				ImGui::Text("Max FrameTime: %f", maxFrameTime);
				ImGui::Text("Avg FrameTime: %f", averageFrameTime);
				ImGui::PlotLines("FrameTime", frameTimes, halfMaxFrameTimes, currFrame % halfMaxFrameTimes, nullptr, 1.0f / maxDisplayFPS, 1.0f / minDisplayFPS, ImVec2(600, 90.0f));
				ImGui::TreePop();
			}
			if (aWindowName != nullptr)
			{
				ImGui::End();
			}
		}
	}
}
