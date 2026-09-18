#pragma once

#include <filesystem>

#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include "Core/Math/Vector.h"
#include "Engine/Renderer/VertexSimple.h"

// Cooked 메시가 사용하는 CPU 재질 데이터.
// 셰이더·SRV 등 GPU 리소스는 포함하지 않는다.
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

struct FMeshSection
{
    // Indices 배열의 시작 위치. 바이트 단위가 아니다.
    uint32 FirstIndex = 0;

    // 삼각형 수가 아닌 인덱스 수.
    uint32 IndexCount = 0;

    // Materials 배열의 인덱스.
    uint32 MaterialIndex = 0;

    //객체 인덱스
    int32 ObjectIndex = -1;

};

struct FStaticMeshData
{
    FString PathFileName;

    TArray<FNormalVertex> Vertices;
    TArray<uint32> Indices;

    TArray<FMeshSection> Sections;
    TArray<FStaticMeshMaterial> Materials;

    TArray<FStaticMeshObjectInfo> Objects;

    // 메시 로컬 좌표 기준.
    FVector BoundsMin{};
    FVector BoundsMax{};
};