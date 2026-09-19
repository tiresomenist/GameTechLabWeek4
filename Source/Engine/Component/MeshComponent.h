#pragma once
#include "Engine/Component/Primitive/PrimitiveComponent.h"
#include "Core/Name/Name.h"

class UMeshComponent : public UPrimitiveComponent
{
    UCLASS(UMeshComponent, "MeshComponent", UPrimitiveComponent)

public:
    //virtual FMeshResource* GetMeshResource() const override;
    virtual void Serialize(FArchive& Archive) override;
    virtual void Deserialize(FArchive& Archive) override;

    // override material 설정 - 여기서 수정해도 실제 staticmesh는 바뀌지 않음
    void SetOverrideMaterial(FMaterial* InMaterial, uint32 MaterialSlot);
    virtual const FMaterial* GetMaterial(uint32 MaterialSlot) { return OverrideMaterialList[MaterialSlot]; }
    
    virtual void CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected = false) override;

    bool IsVisible() { return bIsVisible; }
    void SetVisibility(bool InVisibility) { bIsVisible = InVisibility; }
private:
    bool bIsVisible = true;
    TArray<FMaterial*> OverrideMaterialList;
};
