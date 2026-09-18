#include "pch.h"
#include "SceneWindow.h"

#include <algorithm>
#include <cmath>

#include "Editor/Editor.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Core/Container/Array.h"
#include "Engine/Component/CameraComponent.h"
#include "Engine/Component/Primitive/FlipbookComponent.h"
#include "Engine/Component/Primitive/TextComponent.h"
#include "Engine/Component/Light/SpotLightComponent.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_stdlib.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Memory/Allocator.h"
#include "Engine/Renderer/ViewSettings.h"
#include "Engine/Scene/SceneManager.h"
#include "Core/Util/File.h"
#include "Editor/Util/MeshSelection.h"
#include "SolarSystem.h"

void USceneWindow::SpawnStaticMesh()
{
	Editor->SpawnStaticMesh(SelectedMeshKey, NumberOfSpawn);
}

void USceneWindow::SpawnSpecialComponent()
{
	Editor->SpawnComponent(SelectedSpecialComponentClass, NumberOfSpawn);
}

void USceneWindow::SpawnEmptyActor()
{
	Editor->CreateEmptyActor();
}

void USceneWindow::InitializeWindow(FEditor* Editor, const FString& Name)
{
	UEditorWindow::InitializeWindow(Editor, Name);

	SpecialComponentClasses.Add(UTextComponent::GetClass());
	SpecialComponentClasses.Add(UFlipbookComponent::GetClass());
	SpecialComponentClasses.Add(USpotLightComponent::GetClass());

	SelectedSpecialComponentClass = *SpecialComponentClasses.begin();

	SelectedMeshKey = MeshSelection::GetEntries()[0].Key;
}


void USceneWindow::Render(float DeltaTime)
{
	if (!bOpen)
	{
		return;
	}

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const ImVec2 WorkPosition = Viewport->WorkPos; // 메뉴창을 제외한 제일 왼쪽 위 위치
	const ImVec2 WorkSize = Viewport->WorkSize;    // 메뉴창을 제외한 Imgui를 띄울 수 있는 공간

	// 전체 프로그램 창 크기에 대한 비율
	constexpr float WindowWidthRatio = 0.42f;
	constexpr float WindowHeightRatio = 0.36f;

	float WindowWidth = WorkSize.x * WindowWidthRatio;
	float WindowHeight = WorkSize.y * WindowHeightRatio;

	ImGui::SetNextWindowPos(
		WorkPosition,
		ImGuiCond_FirstUseEver
	);

	ImGui::SetNextWindowSize(
		ImVec2(WindowWidth, WindowHeight),
		ImGuiCond_FirstUseEver
	);
	
	ImVec2 Available = ImGui::GetContentRegionAvail();
	//float Scale = std::clamp(WindowWidth / 400.0f, 0.1f, 5.0f);
	//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(3.0f * Scale , 2.0f * Scale)); // 버튼 안쪽 여백 증가
	const ImGuiStyle& Style = ImGui::GetStyle();
	ImVec2 ItemSpacing = Style.ItemSpacing; // 아이템간 패딩 값

	float ButtonWidth = Available.x * 0.2f; // Button, DragFloat
	float WideItemWidth = ButtonWidth * 3.0f + ItemSpacing.x * 2.0f; // FOV, NumberOfSpawn

	size_t AllocationBytes = GAllocator::GetTotalAllocationBytes();
	size_t AllocationCount = GAllocator::GetTotalAllocationCount();

	ImGui::Begin(Name.c_str(), &bOpen, ImGuiWindowFlags_HorizontalScrollbar);
	{
		float MilliSeconds = DeltaTime * 1000;
		ImGui::Text("PEPE Engine");
		ImGui::Text("FPS %.00f (%.00f ms)", 1000 / MilliSeconds, MilliSeconds);
		ImGui::Separator();
		ImGui::Text("UObject Heap Memory 사용량: %zu바이트", AllocationBytes);
		ImGui::Text("UObject Heap Memory 객체 수: %zu개", AllocationCount);
		ImGui::Separator();

		ImGui::PushItemWidth(WideItemWidth);

		MeshSelection::DrawCombo("Static Mesh",SelectedMeshKey,ImGuiComboFlags_HeightSmall);

		ImGui::PopItemWidth();
		if (ImGui::Button("Spawn Static Mesh"))
		{
			SpawnStaticMesh();
		}
		ImGui::SameLine();
		ImGui::PushItemWidth(200);
		if (ImGui::InputScalar(
			"Number Of Spawn",
			ImGuiDataType_U32,
			&NumberOfSpawn,
			&Step))
		{
			NumberOfSpawn = std::clamp(NumberOfSpawn, 1u, 20u);
		}
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(WideItemWidth);
		if (ImGui::BeginCombo(
			"Special Component",
			SelectedSpecialComponentClass->DisplayName.c_str(),
			ImGuiComboFlags_HeightSmall))
		{
			for (FClassType* ComponentClass : SpecialComponentClasses)
			{
				const bool bSelected = SelectedSpecialComponentClass == ComponentClass;
				if (ImGui::Selectable(ComponentClass->DisplayName.c_str(), bSelected))
				{
					SelectedSpecialComponentClass = ComponentClass;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		if (ImGui::Button("Spawn Special Component"))
		{
			SpawnSpecialComponent();
		}
		ImGui::SameLine();
		if (ImGui::Button("Create Empty Actor"))
		{
			SpawnEmptyActor();
		}

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
