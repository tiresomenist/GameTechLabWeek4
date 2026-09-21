#pragma once
#include "Engine/Component/Primitive/PrimitiveComponent.h"
#include "Core/Name/Name.h"

class UMeshComponent : public UPrimitiveComponent
{
    UCLASS(UMeshComponent, "MeshComponent", UPrimitiveComponent)

public:
    //virtual FMeshResource* GetMeshResource() const override;
    virtual void Serialize(FArchive& Archive) override;

    virtual FMeshResource* GetMeshResource() const override { return nullptr; }

    // override material 설정 - 여기서 수정해도 실제 staticmesh의 material은 바뀌지 않음
    void SetOverrideMaterial(FMaterial* InMaterial, uint32 MaterialSlot);
    void SetOverrideMaterial(const FString& InMaterialPath, uint32 MaterialSlot = 0);

    virtual const FString& GetMaterialPath(uint32 MaterialSlot = 0) const;
    virtual const FMaterial* GetMaterial(uint32 MaterialSlot = 0) const;
    
    virtual void CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected = false) override;

    bool IsVisible() { return bIsVisible; }
    void SetVisibility(bool InVisibility) { bIsVisible = InVisibility; }
protected:
    TArray<FMaterial*> OverrideMaterialList;
private:
    bool bIsVisible = true;
};
