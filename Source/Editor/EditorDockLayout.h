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
		const ImGuiID ViewportToolbarID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Up, 0.05f, nullptr, &MainID);

		ImGui::DockBuilderDockWindow("Outliner", OutlinerID);
		ImGui::DockBuilderDockWindow("Property Window", PropertyID);
		ImGui::DockBuilderDockWindow("Console", ConsoleID);
		ImGui::DockBuilderDockWindow("Scene Control Panel", SceneID);

		if (ImGuiDockNode* ToolbarNode =
			ImGui::DockBuilderGetNode(ViewportToolbarID))
		{
			ToolbarNode->LocalFlags |=
				ImGuiDockNodeFlags_NoResize |            // toolbar와 중앙 viewport 사이 splitter 고정
				ImGuiDockNodeFlags_NoUndocking |         // toolbar를 끌어내 독립 창으로 만들 수 없음
				ImGuiDockNodeFlags_NoDockingSplit |      // toolbar 영역을 다시 분할할 수 없음
				ImGuiDockNodeFlags_NoDockingOverMe |     // 다른 창을 toolbar에 탭으로 도킹할 수 없음
				ImGuiDockNodeFlags_NoTabBar |            // dock tab/title 영역 제거
				ImGuiDockNodeFlags_NoWindowMenuButton |  // 좌상단 메뉴/접기 버튼 제거
				ImGuiDockNodeFlags_NoCloseButton;        // 닫기 버튼 제거

			ToolbarNode->UpdateMergedFlags();
		}
		ImGui::DockBuilderDockWindow("Viewport Toolbar", ViewportToolbarID);

		ImGui::DockBuilderFinish(DockSpaceID);
		bFirstTime = false;
	}
}
