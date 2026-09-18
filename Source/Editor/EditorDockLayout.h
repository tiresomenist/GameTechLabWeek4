#pragma once

#include "ImGui/imgui_internal.h"

namespace EditorDockLayout
{
	inline ImGuiID GetDockSpaceID() { return ImHashStr("EditorDockSpace"); }

	inline void BuildDefaultLayout(bool bReset)
	{
		static bool bFirstTime = true;
		if (!bReset && !bFirstTime)
		{
			return;
		}

		const ImGuiID DockSpaceID = GetDockSpaceID();

		ImGui::DockBuilderRemoveNode(DockSpaceID);
		ImGui::DockBuilderAddNode(DockSpaceID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(DockSpaceID, ImGui::GetMainViewport()->WorkSize);

		ImGuiID MainID = DockSpaceID;

		ImGuiID RightID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Right, 0.2f, nullptr, &MainID);
		const ImGuiID PropertyID = ImGui::DockBuilderSplitNode(RightID, ImGuiDir_Down, 0.7f, nullptr, &RightID);
		const ImGuiID OutlinerID = RightID;
		const ImGuiID ConsoleID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Down, 0.2f, nullptr, &MainID);
		const ImGuiID SceneID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Left, 0.2f, nullptr, &MainID);

		ImGui::DockBuilderDockWindow("Outliner", OutlinerID);
		ImGui::DockBuilderDockWindow("Property Window", PropertyID);
		ImGui::DockBuilderDockWindow("Console", ConsoleID);
		ImGui::DockBuilderDockWindow("Scene Control Panel", SceneID);

		ImGui::DockBuilderFinish(DockSpaceID);
		bFirstTime = false;
	}
}
