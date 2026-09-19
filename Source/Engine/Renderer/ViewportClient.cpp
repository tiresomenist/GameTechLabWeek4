#include "pch.h"
#include "ViewportClient.h"

void FViewportClient::Initialize(EViewportType InType, UCameraComponent* InCamera)
{
	ViewportType = InType;
	Camera = InCamera;
	switch (ViewportType)
	{
	case EViewportType::Perspective:
	{
		Camera->SetIsPerspective(true);
		Camera->SetRelativeLocation(FVector(-15.0f, -15.0f, 10.0f));
		Camera->LookAt(FVector(0.0f, 0.0f, 0.0f));

		ViewSetting.ViewMode = EViewModeIndex::VMI_Lit;
		break;
	}
	case EViewportType::Top:
	{
		Camera->SetIsPerspective(false);
		Camera->SetRelativeRotation(FQuaternion(0.0f, 90.0f, 0.0f));
		Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
		ViewSetting.ViewMode = EViewModeIndex::VMI_Unlit;
		break;
	}
	case EViewportType::Front:
	{
		Camera->SetIsPerspective(false);
		Camera->SetRelativeRotation(FQuaternion(0.0f, 0.0f, 180.0f));
		Camera->SetRelativeLocation(FVector(30.0f, 0.0f, 0.0f));
		ViewSetting.ViewMode = EViewModeIndex::VMI_Unlit;
		break;
	}
	case EViewportType::Right:
	{
		Camera->SetIsPerspective(false);
		Camera->SetRelativeRotation(FQuaternion(0.0f, 0.0f, 90.0f));
		Camera->SetRelativeLocation(FVector(0.0f, 30.0f, 0.0f));
		ViewSetting.ViewMode = EViewModeIndex::VMI_Unlit;
		break;
	}
	}
}

void FViewportClient::SetRect(float x, float y, float Width, float Height)
{
	ViewportInfo.TopLeftX = x;
	ViewportInfo.TopLeftY = y;
	ViewportInfo.Width = Width;
	ViewportInfo.Height = Height;
}

FRenderView FViewportClient::GetRenderView() const
{
	FRenderView result;
	result.bDrawEditorGizmos = bDrawEditorGizmos;
	result.Camera = Camera;
	result.ViewSettings = ViewSetting;
	result.Viewport = ViewportInfo;

	return result;
}

UCameraComponent* FViewportClient::GetCamera() const
{
	return Camera;
}

bool FViewportClient::IsMouseInside(float ScreenX, float ScreenY) const
{
	float LeftX = ViewportInfo.TopLeftX;
	float RightX = ViewportInfo.TopLeftX + ViewportInfo.Width;

	float TopY = ViewportInfo.TopLeftY;
	float BottomY = ViewportInfo.TopLeftY + ViewportInfo.Height;

	return (ScreenX >= LeftX) && (ScreenX < RightX) && (ScreenY >= TopY) && (ScreenY < BottomY);
}
