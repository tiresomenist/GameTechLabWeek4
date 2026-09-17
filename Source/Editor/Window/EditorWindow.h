#pragma once

#include "Engine/Engine.h"
#include "Engine/Object/Object.h"
#include "Imgui/imgui.h"
#include <Windows.h>

class FEditor;

class UEditorWindow : public UObject
{
    UCLASS(UEditorWindow, "EditorWindow", UObject)

protected:

	FEditor* Editor = nullptr;

public:

	virtual void Initialize(FEditor* InEditor);

	virtual void Render(float DeltaTime) {}

	void DrawItemBottomLine(uint32 Color, float Thickness);

	virtual ~UEditorWindow() override = default;
};

