#pragma once

#include "Editor/Window/EditorWindow.h"

class AActor;
class USceneComponent;

class UOutlinerWindow : public UEditorWindow
{
	UCLASS(UOutlinerWindow, "OutlinerWindow", UEditorWindow)

public:
	void InitializeWindow(FEditor* InEditor, const FString& InName) override;

	virtual void Render(float DeltaTime) override;

private:
	void DrawActorTree(AActor* Actor);
	void SetVisibilitySubtree(AActor* Actor, bool bVisible);

	const char* ActorNameBuffer;
};
