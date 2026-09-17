#pragma once

#include "Core/Container/Array.h"
#include "Engine/Renderer/Text/WorldTextItem.h"
#include "Engine/Renderer/Line/LineBatcher.h"
#include "Engine/Renderer/Line/LineDrawRequest.h"
class UScene;
class FEditor;
class UCameraComponent;
struct FPrimitiveRenderData;

namespace RenderUtil
{
	TArray<FPrimitiveRenderData> GetRenderList(FEditor* Editor, UScene* Scene);
	TArray<FPrimitiveRenderData> GetGizmoList(FEditor* Editor, UScene* Scene);
	TArray<FWorldTextItem> GetTextRenderList(UScene* Scene, const UCameraComponent* Camera, bool bShowUUIDWidgets);
	void SubmitLineDrawRequests(FEditor* Editor,UScene* Scene,FLineBatcher& Batcher);
};
