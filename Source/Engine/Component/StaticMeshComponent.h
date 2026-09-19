#pragma once
#include "MeshComponent.h"
#include "Core/Name/Name.h"
#include "Engine/Resource/StaticMesh.h"

class UStaticMeshComponent : public UMeshComponent
{
    UCLASS(UStaticMeshComponent, "StaticMeshComponent", UMeshComponent)

public:
    void SetStaticMesh(const FName& InMeshKey);
    //virtual FMeshResource* GetMeshResource() const override;
    virtual void Serialize(FArchive& Archive) override;
    virtual void Deserialize(FArchive& Archive) override;

    void SetMaterial(FMaterial* InMaterial, uint32 MaterialSlot);

    const FName& GetStaticMeshKey() const { return MeshKey; }
    const FString& GetMaterialPath() const { return MaterialPath; }
    const FMaterial* GetMaterial(uint32 MaterialSlot) { return MaterialList[MaterialSlot]; }

    virtual void CreateRenderData(bool bSelected = false, TArray<FPrimitiveRenderData>& ComponentRenderData) override;

private:
    FName MeshKey;
    FString MaterialPath;
    TArray<FMaterial*> MaterialList;
};
