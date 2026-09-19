#pragma once

#include <algorithm>
#include <cmath>

#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include "Engine/Object/ObjectFactory.h"
#include "Engine/Renderer/RenderUtil.h"
#include "Editor/Controller/CameraController.h"
#include "Editor/Controller/GizmoController.h"

//TESTCODE//
#include "EditorMenuLayout.h"
#include "Engine/Component/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/Console.h"
#include "Engine/Renderer/ViewSettings.h"

#include "Engine/Renderer/Grid.h"
#include "Core/Name/Name.h"

#include "Engine/Renderer/ViewportClient.h"


class USceneComponent;
class UCameraComponent;
class AActor;
class UEditorWindow;
class UGizmo;
class UGrid;
class FObjectPicker;
class FGizmoPicker;

class FEditor
{
private:
	// 현재 선택된 SceneComponent
	UCameraComponent* EditorCamera = nullptr;
	FCameraController CameraController;
	FObjectPicker* ObjectPicker = nullptr;
	FGizmoPicker* GizmoPicker = nullptr;
	FGizmoController* GizmoController = nullptr;

	AActor* SelectedActor = nullptr;
	UActorComponent* SelectedComponent = nullptr;
	FViewSettings ViewSettings;
	FGrid Grid;
	TArray<UGizmo*> Gizmos;
	FEditorMenuLayout MenuLayout;
	TArray<UEditorWindow*> Windows;
	TArray<UGrid*> Grids;
	UGizmo* ObjectAxisGizmo = nullptr;

	// Viewport 배열
	TArray<FViewportClient> Viewports;
	uint32 CurrEditedViewportIndex = 0;

	//예외처리용 초기화 여부
	bool bInitialized = false;
	bool bCanSaveEditorSettings = false;

	// 이전 프레임 마우스 눌림상태 저장 - 드래그 중 뷰포트 변경 방지
	bool bPrevLDown = false;
	bool bPrevRDown = false;

	void InitializeGizmos();
	void InitializeWindows();
	void InitializeGrids();

	void ReleaseGizmos();
	void ReleaseWindows();
	void ReleaseGrids();

public:

	void Initialize();

	void Tick(float DeltaTime);

	void Release();

	void SpawnStaticMesh(const FName& MeshKey, int Count);
	void SpawnComponent(FClassType* ComponentClass, int Count);
	void CreateEmptyActor();

	void NewScene();
	void LoadScene(FStringView SceneName);
	void LoadSceneFromPath(const std::filesystem::path& ScenePath);
	void SaveScene(FStringView SceneName);

	UScene* GetCurrentScene();
	UCameraComponent* GetEditorCamera() { return EditorCamera; }
	const FViewSettings& GetViewSettings()const { return ViewSettings; }

	void SetSelectedActor(AActor* Actor);
	AActor* GetSelectedActor() const { return SelectedActor; }
	void SetSelectedComponent(UActorComponent* Component);
	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }
	USceneComponent* GetSelectedSceneComponent() const { return dynamic_cast<USceneComponent*>(SelectedComponent); }
	USceneComponent* GetTransformTarget() const;
	
	//내부적으로 비트마스킹으로 처리해줌.
	bool IsShowingUUIDLabels() const { return ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::UUID); }
	void SetShowUUIDLabels(bool bShow) { ViewSettings.ShowFlags.SetEnabled(EEngineShowFlag::UUID, bShow); }
	bool IsShowingBoundingBoxes() const { return ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::Bounds); }
	void SetShowBoundingBoxes(bool bShow) { ViewSettings.ShowFlags.SetEnabled(EEngineShowFlag::Bounds, bShow); }
	EViewModeIndex GetViewMode() const { return ViewSettings.ViewMode; }
	void SetViewMode(EViewModeIndex InMode) { ViewSettings.ViewMode = InMode; }
	bool IsShowingPrimitives() const{return ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::Primitives);}
	void SetShowPrimitives(bool bShow){ViewSettings.ShowFlags.SetEnabled(EEngineShowFlag::Primitives, bShow);}
	bool IsShowingGrid() const{return ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::Grid);}
	void SetShowGrid(bool bShow){ViewSettings.ShowFlags.SetEnabled(EEngineShowFlag::Grid, bShow);}
	bool IsShowingWorldAxis() const { return ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::WorldAxis); }
	void SetShowWorldAxis(bool bShow) { ViewSettings.ShowFlags.SetEnabled(EEngineShowFlag::WorldAxis, bShow); }
	const FGrid& GetGrid() const { return Grid; }
	void SetGridInterval(float InInterval)
	{
		if (std::isfinite(InInterval))
		{
			Grid.Interval = std::clamp(InInterval, FGrid::MinInterval, FGrid::MaxInterval);
		}
	}
	int32 GetActiveGizmoAxis() const { return GizmoController ? GizmoController->GetActiveAxis() : -1; }

	void RemoveSelectedComponent();
	void DeleteSelectedActor();

	void RegisterGizmo(FClassType* Type);
	void RegisterWindow(FClassType* Type, const FString& Name);
	void RegisterGrid(FClassType* Type);

	void LoadEditorSetting();

	void SaveEditorSetting();

	const TArray<UGizmo*>& GetGizmos() const { return Gizmos; }
	FEditorMenuLayout& GetMenuLayout() { return MenuLayout; }
	const TArray<UEditorWindow*>& GetWindows() const { return Windows; }
	const TArray<UGrid*>& GetGrids() const { return Grids; }

	//TEST CODE//
	FVector GetCameraLocation() { return GetEditorCamera()->GetRelativeLocation(); }
	void SetCameraLocation(FVector NewCameraLocation) { EditorCamera->SetRelativeLocation(NewCameraLocation); }
	FVector GetCameraRotationDegree()
	{
		const FRotator& CameraRotation = GetEditorCamera()->GetRelativeRotator();
		return CameraRotation.ToEulerDegrees();
	}
	void SetCameraRotationDegree(const FVector& NewRotationDegree)
	{
		GetEditorCamera()->SetRelativeRotation(FRotator::FromEulerDegrees(NewRotationDegree));
	}
	float GetCameraFOV() { return GetEditorCamera()->GetFOV() * 180.0f / PI; }
	void SetCameraFOV(float NewFOV) { EditorCamera->SetFOVByDegree(NewFOV); }
	
	void SpawnPrimitives(FClassType* ClassType, uint32 num) { GEngine::GetInstance()->GetConsole()->Append(std::format("Make {}, {} times",ClassType->DisplayName,num)); }
	
	void SetObjectAxisGizmo(UGizmo* InGizmo);
	UGizmo* GetObjectAxisGizmo()const;

	UObject* SpawnObject(FClassType* Type);

	// 뷰포트 사이즈 설정용 함수
	void OnResize(uint32 Width, uint32 Height);

	const TArray<FViewportClient>& GetViewports();
	const uint32 GetCurrentEditViewportIndex() { return CurrEditedViewportIndex; }

public:
	friend TArray<FPrimitiveRenderData> RenderUtil::GetRenderList(FEditor* Editor, UScene* Scene, const UCameraComponent* Camera);
	friend TArray<FPrimitiveRenderData> RenderUtil::GetGizmoList(FEditor* Editor, UScene* Scene,const UCameraComponent* Camera,
		const D3D11_VIEWPORT& Viewport);
};
