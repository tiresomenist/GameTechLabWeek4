#pragma once

#include "Engine/Component/CameraComponent.h"
#include "ViewRenderer.h"
#include "ViewSettings.h"

enum class EViewportType
{
	Perspective,
	Top,
	Front,
	Right
};

class FViewportClient
{
public:
	void Initialize(EViewportType InType, UCameraComponent* InCamera);
	void SetRect(float x, float y, float Width, float Height);
	FRenderView GetRenderView() const;

	UCameraComponent* GetCamera() const;
	bool IsMouseInside(float ScreenX, float ScreenY) const;
private:
	UCameraComponent* Camera = nullptr;
	EViewportType ViewportType = EViewportType::Perspective;
	FViewSettings ViewSetting;
	D3D11_VIEWPORT ViewportInfo;
	bool bDrawEditorGizmos = false;
};