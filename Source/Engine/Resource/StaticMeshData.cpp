#include "pch.h"
#include "Engine/Resource/StaticMeshData.h"
#include "Core/Serialization/Archive.h"
#include "Core/Serialization/WindowsBinReader.h"
#include "Core/Serialization/WindowsBinWriter.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    constexpr uint32 MeshMagic = 0x4853454Du; // little-endian으로 "MESH"
    constexpr uint32 MeshVersion = 1;

    // 메시 파일의 식별자와 데이터 버전을 저장하거나 검사한다.
    void SerializeMeshHeader(FArchive& Archive)
    {
        // 공통 바이너리 헤더와 별개로 본문이 메시 데이터인지 구분한다.
        uint32 Magic = MeshMagic;
        uint32 Version = MeshVersion;
        Archive.Field("MeshMagic", Magic);
        Archive.Field("MeshVersion", Version);
        if (Magic != MeshMagic)
            throw std::runtime_error("Invalid static mesh signature.");
        if (Version != MeshVersion)
            throw std::runtime_error("Unsupported static mesh version.");
    }

    // 벡터의 세 성분을 필수 필드로 저장하거나 복원한다.
    void SerializeVector(FArchive& Archive, const char* Name, FVector& Value)
    {
        // 메시 데이터에는 기본값 보정 없이 세 성분이 모두 있어야 한다.
        if (!Archive.BeginObject(Name))
            throw std::runtime_error("Missing mesh vector.");
        Archive.Field("X", Value.X);
        Archive.Field("Y", Value.Y);
        Archive.Field("Z", Value.Z);
        Archive.EndObject();
    }

    // 파일 경로를 UTF-8 문자열로 저장하거나 원래 경로 타입으로 복원한다.
    void SerializePath(FArchive& Archive, const char* Name, std::filesystem::path& Value)
    {
        FString Text;

        // 운영체제의 좁은 문자열 인코딩에 의존하지 않고 UTF-8로 변환한다.
        if (Archive.IsSaving())
        {
            const auto Utf8 = Value.u8string();
            Text.assign(Utf8.begin(), Utf8.end());
        }
        Archive.Field(Name, Text);
        if (Archive.IsLoading())
        {
            const std::u8string Utf8(Text.begin(), Text.end());
            Value = std::filesystem::path(Utf8);
        }
    }

    // 정점의 위치, 법선, 색상, UV를 정해진 순서로 처리한다.
    void SerializeVertex(FArchive& Archive, FVertexPNCT& Vertex)
    {
        // 메모리 구조체 전체가 아닌 실제 데이터 성분만 기록한다.
        Archive.Field("X", Vertex.x);
        Archive.Field("Y", Vertex.y);
        Archive.Field("Z", Vertex.z);
        Archive.Field("NX", Vertex.nx);
        Archive.Field("NY", Vertex.ny);
        Archive.Field("NZ", Vertex.nz);
        Archive.Field("R", Vertex.r);
        Archive.Field("G", Vertex.g);
        Archive.Field("B", Vertex.b);
        Archive.Field("A", Vertex.a);
        Archive.Field("U", Vertex.u);
        Archive.Field("V", Vertex.v);
    }

    // 섹션의 인덱스 범위와 재질 및 객체 참조를 처리한다.
    void SerializeSection(FArchive& Archive, FMeshSection& Section)
    {
        // 현재 Cook이 생성하는 섹션 필드를 같은 순서로 유지한다.
        Archive.Field("FirstIndex", Section.FirstIndex);
        Archive.Field("IndexCount", Section.IndexCount);
        Archive.Field("MaterialIndex", Section.MaterialIndex);
        Archive.Field("ObjectIndex", Section.ObjectIndex);
    }

    // CPU 재질의 이름, 색상, 불투명도, 텍스처 경로를 처리한다.
    void SerializeMaterial(FArchive& Archive, FStaticMeshMaterial& Material)
    {
        // GPU 텍스처 대신 원본 텍스처를 찾을 경로만 기록한다.
        Archive.Field("Name", Material.Name);
        SerializeVector(Archive, "DiffuseColor", Material.DiffuseColor);
        Archive.Field("Opacity", Material.Opacity);
        SerializePath(Archive, "DiffuseTexturePath", Material.DiffuseTexturePath);
    }

    // 메시 객체의 이름을 저장하거나 복원한다.
    void SerializeObjectInfo(FArchive& Archive, FStaticMeshObjectInfo& Object)
    {
        Archive.Field("Name", Object.Name);
    }

    // 구조체 배열을 원소별 직렬화 함수로 처리한다.
    template<typename T>
    void SerializeStructArray(FArchive& Archive, const char* Name, TArray<T>& Values,
        size_t MinimumElementBytes, void (*SerializeItem)(FArchive&, T&))
    {
        uint32 Count = Archive.IsSaving() ? static_cast<uint32>(Values.Num()) : 0;
        if (!Archive.BeginArray(Name, Count))
            throw std::runtime_error(FString("Missing mesh array: ") + Name);
        if (Count > static_cast<uint32>((std::numeric_limits<int32>::max)()))
            throw std::length_error("Mesh array exceeds TArray capacity.");

        // 파일에서 읽은 개수가 실제 남은 데이터로 구성 가능한지 먼저 검사한다.
        if (Archive.IsLoading())
        {
            Archive.CheckArraySize(Count, MinimumElementBytes);
            Values.SetNum(Count);
        }

        // 배열 원소마다 객체 범위를 열어 JSON Archive에서도 사용할 수 있게 한다.
        for (uint32 Index = 0; Index < Count; ++Index)
        {
            Archive.BeginArrayElement(Index);
            if (!Archive.BeginObject(nullptr))
                throw std::runtime_error("Missing mesh array element.");
            SerializeItem(Archive, Values[Index]);
            Archive.EndObject();
            Archive.EndArrayElement();
        }
        Archive.EndArray();
    }

    // 벡터의 모든 성분이 유한한 수치인지 검사한다.
    bool IsFiniteVector(const FVector& Value)
    {
        return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
    }

    // CPU 메시의 수치와 배열 참조 관계를 검사한다.
    void ValidateMeshData(const FStaticMeshData& Data)
    {
        // 기본 생성된 빈 메시도 허용하되 정점과 인덱스의 존재 여부는 일치해야 한다.
        if (Data.Vertices.IsEmpty() != Data.Indices.IsEmpty())
            throw std::runtime_error("Mesh vertices and indices are inconsistent.");
        if (Data.Indices.Num() % 3 != 0)
            throw std::runtime_error("Mesh index count must be a multiple of three.");

        // Bounds는 유한해야 하며 각 축의 최소값이 최대값보다 클 수 없다.
        if (!IsFiniteVector(Data.BoundsMin) || !IsFiniteVector(Data.BoundsMax)
            || Data.BoundsMin.X > Data.BoundsMax.X
            || Data.BoundsMin.Y > Data.BoundsMax.Y
            || Data.BoundsMin.Z > Data.BoundsMax.Z)
            throw std::runtime_error("Invalid mesh bounds.");

        // 정점의 모든 성분과 Bounds 포함 여부를 검사한다.
        for (const FVertexPNCT& Vertex : Data.Vertices)
        {
            const float Components[] = {
                Vertex.x, Vertex.y, Vertex.z, Vertex.nx, Vertex.ny, Vertex.nz,
                Vertex.r, Vertex.g, Vertex.b, Vertex.a, Vertex.u, Vertex.v
            };
            for (float Component : Components)
            {
                if (!std::isfinite(Component))
                    throw std::runtime_error("Non-finite mesh vertex component.");
            }
            if (Vertex.x < Data.BoundsMin.X || Vertex.x > Data.BoundsMax.X
                || Vertex.y < Data.BoundsMin.Y || Vertex.y > Data.BoundsMax.Y
                || Vertex.z < Data.BoundsMin.Z || Vertex.z > Data.BoundsMax.Z)
                throw std::runtime_error("Mesh vertex lies outside bounds.");
        }

        // 모든 인덱스가 실제 정점을 참조해야 한다.
        const uint32 VertexCount = static_cast<uint32>(Data.Vertices.Num());
        for (uint32 Index : Data.Indices)
        {
            if (Index >= VertexCount)
                throw std::runtime_error("Mesh index is out of range.");
        }

        // Cook 결과와 동일하게 섹션이 전체 인덱스를 연속해서 포함해야 한다.
        const uint32 IndexCount = static_cast<uint32>(Data.Indices.Num());
        uint32 ExpectedFirstIndex = 0;
        for (const FMeshSection& Section : Data.Sections)
        {
            if (Section.FirstIndex != ExpectedFirstIndex
                || Section.IndexCount == 0 || Section.IndexCount % 3 != 0
                || Section.FirstIndex > IndexCount
                || Section.IndexCount > IndexCount - Section.FirstIndex)
                throw std::runtime_error("Invalid mesh section range.");

            if (Section.MaterialIndex >= static_cast<uint32>(Data.Materials.Num()))
                throw std::runtime_error("Mesh section material is out of range.");
            if (Section.ObjectIndex < -1 || Section.ObjectIndex >= Data.Objects.Num())
                throw std::runtime_error("Mesh section object is out of range.");

            ExpectedFirstIndex += Section.IndexCount;
        }
        if (ExpectedFirstIndex != IndexCount)
            throw std::runtime_error("Mesh sections do not cover all indices.");

        // 재질 수치는 검사하지만 텍스처 파일을 실제로 로드하지는 않는다.
        for (const FStaticMeshMaterial& Material : Data.Materials)
        {
            if (!IsFiniteVector(Material.DiffuseColor) || !std::isfinite(Material.Opacity))
                throw std::runtime_error("Non-finite mesh material value.");
        }
    }
}

// Archive의 모드에 따라 모든 CPU 메시 데이터를 저장하거나 복원한다.
void FStaticMeshData::Serialize(FArchive& Archive)
{
    // 고정 순서 바이너리 형식이므로 필드 처리 순서를 유지한다.
    Archive.Field("PathFileName", PathFileName);
    SerializeStructArray(Archive, "Vertices", Vertices, 48, SerializeVertex);
    Archive.Field("Indices", Indices);
    SerializeStructArray(Archive, "Sections", Sections, 16, SerializeSection);
    SerializeStructArray(Archive, "Materials", Materials, 24, SerializeMaterial);
    SerializeStructArray(Archive, "Objects", Objects, 4, SerializeObjectInfo);
    SerializeVector(Archive, "BoundsMin", BoundsMin);
    SerializeVector(Archive, "BoundsMax", BoundsMax);
}

// CPU 메시 데이터를 검사한 뒤 바이너리 파일로 저장한다.
void FStaticMeshData::SaveBinary(const std::filesystem::path& Path)
{
    // 잘못된 메시라면 파일 저장을 시작하기 전에 거부한다.
    ValidateMeshData(*this);

    FWindowsBinWriter Writer;
    SerializeMeshHeader(Writer);
    Serialize(Writer);
    Writer.SaveToFile(Path);
}

// 바이너리 파일을 읽고 검증된 CPU 메시 데이터를 반환한다.
FStaticMeshData FStaticMeshData::LoadBinary(const std::filesystem::path& Path)
{
    // 공통 바이너리 형식에 이어 메시 식별자와 버전을 검사한다.
    auto Reader = FWindowsBinReader::FromFile(Path);
    SerializeMeshHeader(*Reader);

    // 임시 데이터에 완전히 복원하고 검증한 뒤 호출자에게 반환한다.
    FStaticMeshData Data;
    Data.Serialize(*Reader);
    Reader->RequireEnd();
    ValidateMeshData(Data);
    return Data;
}