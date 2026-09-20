#include "pch.h"
#include "EditorMenuLayout.h"

#include "Core/Util/File.h"
#include "Editor/Editor.h"
#include "Editor/Window/EditorWindow.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

void FEditorMenuLayout::Initialize(FEditor* InEditor)
{
	Editor = InEditor;
	SceneName.reserve(128);
}

void FEditorMenuLayout::Draw()
{
	ImGui::BeginMainMenuBar();

	static bool bRequestedLoad = false;
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Scene"))
		{
			NewScene();
		}

		// TODO: 다이얼로그 창으로 변경
		ImGui::PushItemWidth(100.0f);
		ImGui::InputText("Scene Name", SceneName.data(), SceneName.capacity() + 1);
		ImGui::PopItemWidth();

		if (ImGui::MenuItem("Save Scene"))
		{
			SaveScene();
		}

		if (ImGui::MenuItem("Load Scene"))
		{
			bRequestedLoad = true;
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Window"))
	{
		for (UEditorWindow* Window : Editor->GetWindows())
		{
			if (ImGui::MenuItem(Window->GetWindowName().c_str(), nullptr, Window->GetOpenPtr()))
			{
				Window->OpenWindow();
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	BuildDefaultLayout(false);
	ImGui::DockSpaceOverViewport(GetDockSpaceID(), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	const ImGuiDockNode* ViewportNode = ImGui::DockBuilderGetCentralNode(GetDockSpaceID());
	if (ViewportNode)
	{
		const ImVec2 Origin = ImGui::GetMainViewport()->Pos;
		Editor->OnResize(
			ViewportNode->Size.x,
			ViewportNode->Size.y,
			ViewportNode->Pos.x - Origin.x,
			ViewportNode->Pos.y - Origin.y);
	}

	if (bRequestedLoad)
	{
		LoadScene();
		bRequestedLoad = false;
	}
}

void FEditorMenuLayout::NewScene()
{
	Editor->NewScene();
}

void FEditorMenuLayout::SaveScene()
{
	Editor->SaveScene(SceneName);
}

void FEditorMenuLayout::LoadScene()
{
	// imgui_impl_win32가 메인 뷰포트에 HWND를 넣어두므로 그걸 대화상자 owner로 사용
	const HWND Owner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);

	const std::optional<std::filesystem::path> ScenePath = File::OpenFileDialog(Owner, EFileDialogType::Json, "Scenes");
	if (!ScenePath)
	{
		return; // 취소
	}

	// 이후 Save Scene이 같은 이름으로 저장되도록 이름 칸도 갱신
	SceneName = ScenePath->stem().string();
	Editor->LoadSceneFromPath(*ScenePath);
}

void FEditorMenuLayout::BuildDefaultLayout(bool bReset)
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

	if (ImGuiDockNode* OutlinerNode = ImGui::DockBuilderGetNode(OutlinerID))
	{
		OutlinerNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
		OutlinerNode->UpdateMergedFlags();
	}
	ImGui::DockBuilderDockWindow("Outliner", OutlinerID);

	if (ImGuiDockNode* PropertyNode = ImGui::DockBuilderGetNode(PropertyID))
	{
		PropertyNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
		PropertyNode->UpdateMergedFlags();
	}
	ImGui::DockBuilderDockWindow("Properties", PropertyID);

	if (ImGuiDockNode* ConsoleNode = ImGui::DockBuilderGetNode(ConsoleID))
	{
		ConsoleNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
		ConsoleNode->UpdateMergedFlags();
	}
	ImGui::DockBuilderDockWindow("Console", ConsoleID);
	ImGui::DockBuilderDockWindow("Debug", ConsoleID);

	if (ImGuiDockNode* SceneNode = ImGui::DockBuilderGetNode(SceneID))
	{
		SceneNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
		SceneNode->UpdateMergedFlags();
	}
	ImGui::DockBuilderDockWindow("Place Actors", SceneID);

	if (ImGuiDockNode* ToolbarNode =
		ImGui::DockBuilderGetNode(ViewportToolbarID))
	{
		ToolbarNode->LocalFlags |=
			ImGuiDockNodeFlags_NoResize |
			ImGuiDockNodeFlags_NoUndocking |
			ImGuiDockNodeFlags_NoDockingSplit |
			ImGuiDockNodeFlags_NoDockingOverMe |
			ImGuiDockNodeFlags_NoTabBar |
			ImGuiDockNodeFlags_NoWindowMenuButton |
			ImGuiDockNodeFlags_NoCloseButton;

		ToolbarNode->UpdateMergedFlags();
	}
	ImGui::DockBuilderDockWindow("Viewport Toolbar", ViewportToolbarID);

	ImGui::DockBuilderFinish(DockSpaceID);
	bFirstTime = false;
}
