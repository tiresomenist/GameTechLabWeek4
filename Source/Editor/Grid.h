#pragma once

#include "Editor/Editor.h"
#include "Engine/Renderer/PrimitiveRenderData.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Object/Object.h"
#include "Engine/Renderer/Grid.h"
#include "Engine/Renderer/Line/LineDrawRequest.h"

class UGrid : public UObject
{
    UCLASS(UGrid, "Grid", UObject)

private:
    FEditor* Editor = nullptr;

public:
    void Initialize(FEditor* InEditor);

    virtual TArray<FPrimitiveRenderData> GetRenderData();
    FMeshResource* GetMeshResource() { return MeshResource; }
    FLineDrawRequest BuildLineDrawRequest(const FGrid& Grid,const FVector& CameraPosition) const;
    FPrimitiveRenderData RenderData;
    FMeshResource* MeshResource;

    virtual void Render() {};
};