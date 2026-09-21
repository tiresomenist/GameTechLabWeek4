#pragma once

#include <filesystem>

#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include "Core/Math/Vector.h"
#include "Engine/Renderer/VertexSimple.h"
#include "Engine/Resource/MeshSection.h"

class FArchive;

// Cooked 메시가 사용하는 CPU 재질 데이터.
// 셰이더·SRV 등 GPU 리소스는 포함하지 않는다.
// TODO:: 해당 구조체 FMaterial을 직렬화 해서 신정보에 저장할 때 필요할 수도 있음
struct FStaticMeshMaterial
{
    FString Name;

    FVector DiffuseColor{ 1.0f, 1.0f, 1.0f };
    float Opacity = 1.0f;

    std::filesystem::path DiffuseTexturePath;
};

//객체 정보
struct FStaticMeshObjectInfo
{
    FString Name;
};


struct FStaticMeshData
{
    FString PathFileName;

    TArray<FVertexPNCT> Vertices;
    TArray<uint32> Indices;

    TArray<FMeshSection> Sections;
    TArray<FStaticMeshMaterial> Materials;

    TArray<FStaticMeshObjectInfo> Objects;

    // 메시 로컬 좌표 기준.
    FVector BoundsMin{};
    FVector BoundsMax{};

    // Archive의 모드에 따라 모든 CPU 메시 데이터를 저장하거나 복원한다.
    void Serialize(FArchive& Archive);
    // CPU 메시 데이터를 검사한 뒤 바이너리 파일로 저장한다.
    void SaveBinary(const std::filesystem::path& Path);
    // 바이너리 파일을 읽고 검증된 CPU 메시 데이터를 반환한다.
    static FStaticMeshData LoadBinary(const std::filesystem::path& Path);

};