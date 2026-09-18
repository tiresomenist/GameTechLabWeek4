#pragma once

#include "EditorWindow.h"

class UViewportToolbarWindow : public UEditorWindow
{
	UCLASS(UViewportToolbarWindow, "ViewportToolbarWindow", UEditorWindow)

public:
	void Render(float DeltaTime) override;

private:
	void BeginPopupButton(const char* ButtonName, const char* PopupName, const std::function<void()>& DrawFunction);
	void DrawProjectionPopup(UCameraComponent* Camera);
	void DrawViewModePopup();
	void DrawShowFlagsPopup();
};
