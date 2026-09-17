#include "pch.h"
#include "Gizmo.h"
#include "Engine/Object/Object.h"
#include "Engine/Object/ClassType.h"
#include "Engine/Renderer/VertexSimple.h"

void UGizmo::Initialize(FEditor* InEditor)
{
	Editor = InEditor;
}

TArray<FPrimitiveRenderData> UGizmo::GetRenderData()
{
	return TArray<FPrimitiveRenderData>();
}