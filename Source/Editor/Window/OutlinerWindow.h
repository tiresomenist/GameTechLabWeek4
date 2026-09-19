#pragma once

#include "Editor/Window/EditorWindow.h"

class AActor;
class USceneComponent;

class UOutlinerWindow : public UEditorWindow
{
	UCLASS(UOutlinerWindow, "OutlinerWindow", UEditorWindow)

public:
	virtual void Render(float DeltaTime) override;

private:
	void DrawActorTree(AActor* Actor);
	void SetVisibilitySubtree(AActor* Actor, bool bVisible);
	void DrawRenameInput(AActor* Actor, const ImVec2& Position, float Width);

	void RequestRename(AActor* Actor);
	void FinishRename(bool bApply);

	bool CanReparent(AActor* Source, AActor* Target) const;

	AActor* RenameTarget = nullptr;
	FString RenameBuffer;
	bool bFocusRenameInput = false;
	bool bRenameInputDrawn = false;

	AActor* PendingDeleteTarget = nullptr;

	AActor* PendingReparentSource = nullptr;
	AActor* PendingReparentTarget = nullptr;
};
