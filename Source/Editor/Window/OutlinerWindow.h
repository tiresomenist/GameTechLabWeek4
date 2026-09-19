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
};
