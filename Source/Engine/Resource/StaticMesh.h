#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>

#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include "Core/Math/Vector.h"
#include "Engine/Renderer/Material.h"
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

    // FStaticMesh::Materials 배열의 인덱스.
    uint32 MaterialIndex = 0;

    //객체 인덱스
    int32 ObjectIndex = -1;

};

// GPU 정점·인덱스 버퍼를 소유하는 렌더링 리소스.
struct FStaticMesh
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    UINT Stride = 0;
    UINT VertexCount = 0;
    UINT IndexCount = 0;

    TArray<FMeshSection> Sections;
    // 비소유 참조. 재질 소유자는 이 메시를 사용하는 동안 재질의 수명을 보장한다.
    TArray<FMaterial*> Materials;
    TArray<FStaticMeshObjectInfo> Objects;

    FVector BoundsMin;
    FVector BoundsMax;
    bool bHasBounds = false;

    FString PathFileName;
};
