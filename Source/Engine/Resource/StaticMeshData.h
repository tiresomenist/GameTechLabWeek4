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

    // MTL의 Ka·Ks·Ke 값을 각각 보관한다.
    FVector AmbientColor{};
    FVector SpecularColor{};
    FVector EmissiveColor{};

    // MTL의 Ns·Ni·illum 값을 보관한다.
    float SpecularExponent = 0.0f;
    float RefractionIndex = 1.0f;
    int32 IlluminationModel = 0;

    // CPU 재질의 숫자가 보존 가능한 값과 기본 유효 범위를 만족하는지 검사한다.
    bool HasValidNumericValues() const;

};

//객체 정보
struct FStaticMeshObjectInfo
{
    FString Name;
};

// 메시 생성에 사용한 원본 파일의 상태를 보관한다.
struct FStaticMeshSourceFile
{
    // 실제로 읽은 OBJ 또는 MTL의 절대 경로.
    std::filesystem::path FilePath;

    // 파일 크기의 정수값을 손실 없이 보관하는 문자열.
    FString FileSize;

    // 파일 수정 시각의 clock tick 정수값을 보관하는 문자열.
    FString LastWriteTime;
};

struct FStaticMeshData
{
    FString PathFileName;
    TArray<FStaticMeshSourceFile> SourceFiles;
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