#pragma once
#include "EditorWindow.h"

class UStaticOverlayWindow :public UEditorWindow
{
	UCLASS(UStaticOverlayWindow, "StatOverlayWindow", UEditorWindow)
public:
	virtual void Render(float DelatTime) override;
};