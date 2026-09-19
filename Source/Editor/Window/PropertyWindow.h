#pragma once
#include <array>

#include "Editor/Window/EditorWindow.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Rotator.h"
#include "Core/Container/String.h"
#include "Core/Name/Name.h"

class UActorComponent;
class USceneComponent;
class AActor;
struct FClassType;

class UPropertyWindow : public UEditorWindow
{
    UCLASS(UPropertyWindow, "PropertyWindow", UEditorWindow)

private:
	UActorComponent* InspectedComponent = nullptr;
	USceneComponent* TransformTarget = nullptr;
	FVector Translation;
	FRotator RotationDegree;
	FVector OScale;
	bool bEditingRotation = false;
	float SnapSize = 0.001f;
	int SelectedSnapIndex = 0;
	TArray<float> SnapSizeList = {0.001f, 0.01f, 0.1f, 1.0f, 5.0f};
	bool bScaleLock = false;
	TArray<FClassType*> AddableComponentClasses;
	FClassType* SelectedAddComponentClass = nullptr;
	FName SelectedMeshKey;
	AActor* NameEditingActor = nullptr;
	std::array<char, 128> ActorNameBuffer {};

public:
	virtual void InitializeWindow(FEditor* InEditor, const FString& Name) override;

	void GetSelectedValue();
	void SetSelectedValue(bool bSetRotation);
	void RemoveSelectedComponent();
	void DeleteSelectedActor();

	bool DrawRotationField(const char* ID, float& Degree, bool& bRotationActive);
	void RenderComponentTreeSection(AActor* Actor);
	void DrawComponentTree(USceneComponent* Component);
	void RenderAddComponentSection(AActor* Actor);
	bool RenderTransformSection(bool& bRotationActive);
	void RenderSelectedComponentDetails();

	void Render(float DeltaTime) override;

	void DrawComponentContextMenu(UActorComponent* Component);
	void DrawRenameInput(UActorComponent* Component, const ImVec2& Position, float Width);

	void RequestRename(UActorComponent* Component);
	void FinishRename(bool bApply);

	UActorComponent* RenameTarget = nullptr;
	FString RenameBuffer;
	bool bFocusRenameInput = false;
	bool bRenameInputDrawn = false;

	UActorComponent* PendingDeleteTarget = nullptr;
};

