#pragma once
#include "Engine/Component/Primitive/PrimitiveComponent.h"
#include "Core/Name/Name.h"

class UMeshComponent : public UPrimitiveComponent
{
    UCLASS(UMeshComponent, "MeshComponent", UPrimitiveComponent)

public:
    virtual FMeshResource* GetMeshResource() const override;
    virtual void Serialize(FArchive& Archive) override;
    virtual void Deserialize(FArchive& Archive) override;

    void SetMaterial(FMaterial* InMaterial, uint32 MaterialSlot);
    virtual void CreateRenderData(bool bSelected = false, TArray<FPrimitiveRenderData>& ComponentRenderData) override;

    bool IsVisible() { return bIsVisible; }
    void SetVisibility(bool InVisibility) { bIsVisible = InVisibility; }
private:
    bool bIsVisible = true;
    TArray<FMaterial*> MaterialList;
};
