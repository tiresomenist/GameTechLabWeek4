#include "pch.h"
#include "DebugWindow.h"

#include "SolarSystem.h"
#include "Editor/Editor.h"
#include "Engine/Memory/Allocator.h"

void UDebugWindow::Render(float DeltaTime)
{
	if (!bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);

	size_t AllocationBytes = GAllocator::GetTotalAllocationBytes();
	size_t AllocationCount = GAllocator::GetTotalAllocationCount();

	ImGui::Begin(Name.c_str(), &bOpen, ImGuiWindowFlags_HorizontalScrollbar);
	{
		float MilliSeconds = DeltaTime * 1000;
		ImGui::Text("FPS %.00f (%.00f ms)", 1000 / MilliSeconds, MilliSeconds);
		ImGui::Text("UObject Heap Memory 사용량: %zu바이트", AllocationBytes);
		ImGui::Text("UObject Heap Memory 객체 수: %zu개", AllocationCount);

		ImGui::Separator();
		if (ImGui::Button("SpawnSolarSystem"))
		{
			SpawnSolarSystem(Editor->GetCurrentScene());
		}
		if (ImGui::Button("발사"))
		{
			LaunchRocket(Editor->GetCurrentScene());
		}
	}
	ImGui::End();
}
