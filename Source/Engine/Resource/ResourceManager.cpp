#include "pch.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ResourceManager.h"
#include "Engine/Renderer/PrimitiveRenderData.h"
#include "Engine/Renderer/VertexSimple.h"
#include "Engine/Resource/MeshData/Sphere.h"
#include "Engine/Resource/MeshData/Cube.h"
#include "Engine/Resource/MeshData/Plane.h"
#include "Engine/Resource/MeshData/Flame.h"
#include "Engine/Resource/MeshData/Triangle.h"
#include "Engine/Resource/MeshData/PePe.h"
#include "Engine/Resource/MeshData/Octopus.h"
#include "Engine/Resource/MeshData/ArrowRed.h"
#include "Engine/Resource/MeshData/ArrowGreen.h"
#include "Engine/Resource/MeshData/ArrowBlue.h"
#include "Engine/Resource/MeshData/MoveRed.h"
#include "Engine/Resource/MeshData/MoveGreen.h"
#include "Engine/Resource/MeshData/MoveBlue.h"
#include "Engine/Resource/MeshData/ScaleRed.h"
#include "Engine/Resource/MeshData/ScaleGreen.h"
#include "Engine/Resource/MeshData/ScaleBlue.h"
#include "Engine/Resource/MeshData/RotateRed.h"
#include "Engine/Resource/MeshData/RotateGreen.h"
#include "Engine/Resource/MeshData/RotateBlue.h"
#include "Engine/Resource/MeshData/Grid.h"
#include "Engine/Resource/MeshData/rocket_mesh.h"
#include "Core/Math/Vector.h"
#include "Engine/Resource/TextureResource.h"
#include "GeometryGenerator.h"
#include "Engine/Resource/MeshNames.h"
#include "Engine/Log.h"

#include <memory>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <d3dcompiler.h>
#include <format>
namespace
{
    void CheckRenderResourceHR(HRESULT Result, const char* Operation)
    {
        if (FAILED(Result))
        {
            throw std::runtime_error(std::format("{} failed. HRESULT: {}", Operation, Result));
        }
    }

    Microsoft::WRL::ComPtr<ID3DBlob> CompileResourceShader(const WCHAR* FilePath,const char* EntryPoint,
        const char* ShaderModel)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> ShaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;

        const HRESULT Result = D3DCompileFromFile(FilePath,nullptr,nullptr, EntryPoint, ShaderModel,
            0,0,ShaderBlob.GetAddressOf(),ErrorBlob.GetAddressOf());

        if (ErrorBlob)
        {
            UE_LOG("Shader diagnostic: {}",static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }

        CheckRenderResourceHR(Result, "D3DCompileFromFile");
        return ShaderBlob;
    }
}
GResourceManager* GResourceManager::GetInstance()
{
	static GResourceManager Instance;
	return &Instance;
}

void GResourceManager::Initialize(GDevice* InDevice)
{
    Device = InDevice;
	if (!Device || !Device->GetDevice() || !DefaultFont.Build(Device->GetDevice(), "Assets/Fonts/Pretendard-Regular.ttf", 24.0f))
		throw std::runtime_error("Default font atlas build failed");
    RegisterDefaultPrimitives(InDevice);
    RegisterTexturePrimitives(InDevice);
    RegisterDefaultRenderResources();
}

FMeshResource* GResourceManager::CreateMesh(const FName& MeshName,
    std::span<const FVertexSimple> Vertices, std::span<const uint32> Indices)
{
    if (MeshName.IsNone()) { return nullptr; }
    if (FMeshResource** Existing = PrimitiveCache.Find(MeshName)) {
        return *Existing;
    }
    if (Vertices.empty() || Indices.empty()) return nullptr;
    const size_t MaxBytes = (std::numeric_limits<UINT>::max)();
    if (Vertices.size() > MaxBytes / sizeof(FVertexSimple) || Indices.size() > MaxBytes / sizeof(uint32))
        return nullptr;
    for (const auto& V : Vertices)
        if (!std::isfinite(V.x) || !std::isfinite(V.y) || !std::isfinite(V.z)) return nullptr;
    if (!Device || !Device->GetDevice()) return nullptr;
    for (uint32 Index : Indices)
        if (Index >= Vertices.size()) return nullptr;

    auto Mesh = std::make_unique<FMeshResource>();
    // CPU 피킹과 바운딩 계산에 필요한 로컬 위치만 보관함
    for (const auto& Vertex : Vertices)
        Mesh->Positions.Add(FVector(Vertex.x, Vertex.y, Vertex.z));
    Mesh->indexes.GetVector().assign(Indices.begin(), Indices.end());
    Mesh->VertexCount = static_cast<UINT>(Vertices.size());
    Mesh->IndexCount = static_cast<UINT>(Indices.size());
    Mesh->Stride = sizeof(FVertexSimple);
    // GPU에는 색상 정보를 포함한 원본 정점을 업로드함
    Mesh->VertexBuffer = Device->CreateVertexBuffer(Vertices.data(), Mesh->Stride * Mesh->VertexCount);
    if (!Mesh->VertexBuffer) return nullptr;
    Mesh->IndexBuffer = Device->CreateIndexBuffer(&Mesh->indexes[0], sizeof(uint32) * Mesh->IndexCount);
    if (!Mesh->IndexBuffer) return nullptr;
    Mesh->bHasBounds = false;
    if (Mesh->Positions.Num() > 0)
    {
        Mesh->BoundsMin = Mesh->Positions[0];
        Mesh->BoundsMax = Mesh->BoundsMin;

        for (const FVector& Position : Mesh->Positions)
        {
            Mesh->BoundsMin.X = (std::min)(Mesh->BoundsMin.X, Position.X);
            Mesh->BoundsMin.Y = (std::min)(Mesh->BoundsMin.Y, Position.Y);
            Mesh->BoundsMin.Z = (std::min)(Mesh->BoundsMin.Z, Position.Z);

            Mesh->BoundsMax.X = (std::max)(Mesh->BoundsMax.X, Position.X);
            Mesh->BoundsMax.Y = (std::max)(Mesh->BoundsMax.Y, Position.Y);
            Mesh->BoundsMax.Z = (std::max)(Mesh->BoundsMax.Z, Position.Z);
        }

        Mesh->bHasBounds = true;
    }

    if (PrimitiveCache.Add(MeshName, Mesh.get())){
        return Mesh.release();
    }
    // 등록되지 않은 임시 Mesh는 unique_ptr이 해제함
    return GetPrimitive(MeshName);
    
}

FMeshResource* GResourceManager::CreateTexturedMesh(const FName& MeshName,
    std::span<const FVertexTexture> Vertices, std::span<const uint32> Indices)
{
    // 같은 이름의 텍스처 메시를 재사용하며 다른 정점 형식과의 충돌을 거부함
    if (MeshName.IsNone()) { return nullptr; }
    if (FMeshResource** Existing = PrimitiveCache.Find(MeshName)) {
        return (*Existing)->GetStride() == sizeof(FVertexTexture) ? *Existing : nullptr;
    }
    if (!Device || !Device->GetDevice()) return nullptr;
    if (Vertices.empty() || Indices.empty() || Indices.size() % 3 != 0) return nullptr;

    // 버퍼 크기를 UINT로 변환하기 전에 곱셈 오버플로를 검사함
    const size_t VertexCount = Vertices.size();
    const size_t IndexCount = Indices.size();
    const size_t MaxBytes = (std::numeric_limits<UINT>::max)();
    if (VertexCount > MaxBytes / sizeof(FVertexTexture) || IndexCount > MaxBytes / sizeof(uint32))
        return nullptr;

    for (const auto& Vertex : Vertices)
    {
        if (!std::isfinite(Vertex.x) || !std::isfinite(Vertex.y) || !std::isfinite(Vertex.z) ||
            !std::isfinite(Vertex.u) || !std::isfinite(Vertex.v))
            return nullptr;
    }
    for (uint32 Index : Indices)
        if (Index >= VertexCount) return nullptr;

    // 생성 도중 실패하면 이미 생성된 버퍼도 메시 소멸자에서 해제함
    auto Mesh = std::make_unique<FMeshResource>();
    Mesh->VertexCount = static_cast<UINT>(VertexCount);
    Mesh->IndexCount = static_cast<UINT>(IndexCount);
    Mesh->Stride = sizeof(FVertexTexture);
    // 입력은 소유하지 않는 뷰이므로 CPU 피킹용 인덱스는 자체 배열에 복사함
    Mesh->indexes.SetNum(IndexCount);
    std::copy(Indices.begin(), Indices.end(), Mesh->indexes.begin());

    // CPU에는 피킹과 바운딩 계산에 사용하는 위치만 보관함
    Mesh->Positions.SetNum(VertexCount);
    for (size_t Index = 0; Index < VertexCount; ++Index)
    {
        const auto& Vertex = Vertices[Index];
        Mesh->Positions[Index] = FVector(Vertex.x, Vertex.y, Vertex.z);
    }

    // GPU에는 위치와 UV가 포함된 원본 정점을 업로드함
    Mesh->VertexBuffer = Device->CreateVertexBuffer(
        Vertices.data(), static_cast<UINT>(VertexCount * sizeof(FVertexTexture)));
    if (!Mesh->VertexBuffer) return nullptr;

    Mesh->IndexBuffer = Device->CreateIndexBuffer(
        &Mesh->indexes[0], static_cast<UINT>(IndexCount * sizeof(uint32)));
    if (!Mesh->IndexBuffer) return nullptr;

    // 로컬 위치의 축별 최솟값과 최댓값으로 바운딩 박스를 계산함
    Mesh->BoundsMin = Mesh->Positions[0];
    Mesh->BoundsMax = Mesh->Positions[0];
    for (const FVector& Position : Mesh->Positions)
    {
        Mesh->BoundsMin.X = (std::min)(Mesh->BoundsMin.X, Position.X);
        Mesh->BoundsMin.Y = (std::min)(Mesh->BoundsMin.Y, Position.Y);
        Mesh->BoundsMin.Z = (std::min)(Mesh->BoundsMin.Z, Position.Z);
        Mesh->BoundsMax.X = (std::max)(Mesh->BoundsMax.X, Position.X);
        Mesh->BoundsMax.Y = (std::max)(Mesh->BoundsMax.Y, Position.Y);
        Mesh->BoundsMax.Z = (std::max)(Mesh->BoundsMax.Z, Position.Z);
    }
    Mesh->bHasBounds = true;

    if (PrimitiveCache.Add(MeshName, Mesh.get()))
    {
        return Mesh.release();
    }

    // 등록되지 않은 임시 Mesh는 unique_ptr이 해제함
    return GetPrimitive(MeshName);
}

void GResourceManager::Shutdown()
{
    for (auto& [type, mesh] : PrimitiveCache)
    {
        delete mesh;
    }
    for (auto& [Path, Texture] : TextureCache)
    {
        delete Texture;
    }
    TextureCache.Empty();
    PrimitiveCache.Empty();
    DefaultFont.Release();
    TextureMaterialConstantBuffer.Reset();
    WireframePixelShader.Reset();
    ShaderCache.Empty();
    SamplerCache.Empty();
    Device = nullptr;

}

FMeshResource* GResourceManager::GetPrimitive(const FName& MeshName)
{
    if (MeshName.IsNone()){return nullptr;}

    if (FMeshResource** Found = PrimitiveCache.Find(MeshName))
    {
        return *Found;
    }

    return nullptr;
}

FTextureResource* GResourceManager::GetOrLoadTexture(const FString& FilePath)
{
    // 이미 로드한 경로라면 기존 리소스를 반환함
    if (FTextureResource** Existing = TextureCache.Find(FilePath))
    {
        return *Existing;
    }

    if (!Device || !Device->GetDevice())
    {
        throw std::runtime_error("Texture device is not initialized");
    }

    // 로딩이나 캐시 등록이 실패하면 임시 객체를 자동 해제함
    auto NewTexture = std::make_unique<FTextureResource>();

    NewTexture->Load(Device->GetDevice(), FilePath);

    // 로딩에 성공한 리소스만 캐시에 등록함
    if (!TextureCache.Add(FilePath, NewTexture.get()))
    {
        // 같은 키가 이미 존재하면 임시 객체를 해제하고 기존 값을 반환함
        return *TextureCache.Find(FilePath);
    }

    // 캐시 등록 후 소유권을 리소스 매니저로 이전함
    return NewTexture.release();
}

void GResourceManager::RegisterShader(const FName& Name, const WCHAR* FilePath,
    const char* VSEntry, const char* PSEntry, const TArray<D3D11_INPUT_ELEMENT_DESC>& Layout)
{
    if (!Device || !Device->GetDevice())
    {
        throw std::runtime_error("Shader device is not initialized");
    }

    if (Name.IsNone() ||!FilePath || !*FilePath ||!VSEntry || !*VSEntry || !PSEntry || !*PSEntry ||Layout.IsEmpty())
    {
        throw std::invalid_argument("Invalid shader registration");
    }

    if (ShaderCache.Contains(Name))
    {
        throw std::logic_error(std::format("Shader already registered: {}", Name.ToString()));
    }

    ID3D11Device* NativeDevice = Device->GetDevice();

    FShaderResource Resource;

    const auto VSBlob = CompileResourceShader(FilePath, VSEntry, "vs_5_0");

    //버텍스 셰이더 생성 시도
    CheckRenderResourceHR(
        NativeDevice->CreateVertexShader(
            VSBlob->GetBufferPointer(),
            VSBlob->GetBufferSize(),
            nullptr,
            Resource.VertexShader.GetAddressOf()),
        "CreateVertexShader");

    // 인풋 레이아웃 생성 시도
    CheckRenderResourceHR(
        NativeDevice->CreateInputLayout(
            Layout.GetData(),
            static_cast<UINT>(Layout.Num()),
            VSBlob->GetBufferPointer(),
            VSBlob->GetBufferSize(),
            Resource.InputLayout.GetAddressOf()),
        "CreateInputLayout");

    const auto PSBlob =
        CompileResourceShader(FilePath, PSEntry, "ps_5_0");

    // 픽셀 셰이더 생성 시도
    CheckRenderResourceHR(
        NativeDevice->CreatePixelShader(
            PSBlob->GetBufferPointer(),
            PSBlob->GetBufferSize(),
            nullptr,
            Resource.PixelShader.GetAddressOf()),
        "CreatePixelShader");

    if (!ShaderCache.Add(Name, Resource))
    {
        throw std::logic_error("Failed to register shader");
    }
}

const FShaderResource* GResourceManager::GetShader(const FName& Name) const
{
    return ShaderCache.Find(Name);
}

ID3D11PixelShader* GResourceManager::GetWireframePixelShader() const
{
    return WireframePixelShader.Get();
}

void GResourceManager::RegisterSampler(const FName& Name, const D3D11_SAMPLER_DESC& Desc)
{
    if (!Device || !Device->GetDevice())
    {
        throw std::runtime_error("Sampler device is not initialized");
    }

    if (Name.IsNone())
    {
        throw std::invalid_argument("Invalid sampler name");
    }

    if (SamplerCache.Contains(Name))
    {
        throw std::logic_error(std::format("Sampler already registered: {}", Name.ToString()));
    }

    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;

    //샘플러 생성 시도
    CheckRenderResourceHR(
        Device->GetDevice()->CreateSamplerState(
            &Desc,
            Sampler.GetAddressOf()),
        "CreateSamplerState");

    if (!SamplerCache.Add(Name, Sampler))
    {
        throw std::logic_error("Failed to register sampler");
    }
}

ID3D11SamplerState* GResourceManager::GetSampler(const FName& Name) const
{
    const auto* Found = SamplerCache.Find(Name);
    return Found ? Found->Get() : nullptr;
}

FMaterial GResourceManager::CreateColorMaterial() const
{
    static const FName ShaderName("Mesh.Color");

    FMaterial Material{};
    Material.Shader = GetShader(ShaderName);
    return Material;
}

FMaterial GResourceManager::CreateTextureMaterial(ID3D11ShaderResourceView* SRV) const
{
    static const FName ShaderName("Mesh.Texture");
    static const FName SamplerName("LinearClamp");

    FMaterial Material{};
    Material.SRV = SRV;
    Material.Shader = GetShader(ShaderName);
    Material.Sampler = GetSampler(SamplerName);
    Material.ConstantBuffer = TextureMaterialConstantBuffer.Get();
    return Material;
}

void GResourceManager::RegisterDefaultPrimitives(GDevice* InDevice)
{
    const FMeshNames& Names = GetMeshNames();

    //if (!CreateMesh(Names.Triangle, triangle_vertices, triangle_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateTexturedMesh(Names.Flame, flame_vertices, flame_indices)) throw std::runtime_error("Flame mesh creation failed");
    if (!CreateTexturedMesh(Names.Pepe, pepe_vertices, pepe_indices)) throw std::runtime_error("Pepe mesh creation failed");
    if (!CreateMesh(Names.Octopus, octopus_vertices, octopus_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ArrowRed, arrow_red_vertices, arrow_red_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ArrowGreen, arrow_green_vertices, arrow_green_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ArrowBlue, arrow_blue_vertices, arrow_blue_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.MoveRed, move_red_vertices, move_red_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.MoveGreen, move_green_vertices, move_green_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.MoveBlue, move_blue_vertices, move_blue_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ScaleRed, scale_red_vertices, scale_red_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ScaleGreen, scale_green_vertices, scale_green_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.ScaleBlue, scale_blue_vertices, scale_blue_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.RotateRed, rotate_red_vertices, rotate_red_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.RotateGreen, rotate_green_vertices, rotate_green_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.RotateBlue, rotate_blue_vertices, rotate_blue_indices)) throw std::runtime_error("Required mesh creation failed");
    if (!CreateMesh(Names.Grid, grid_vertices, grid_indices)) throw std::runtime_error("Required mesh creation failed");
}


void GResourceManager::RegisterTexturePrimitives(GDevice* InDevice)
{
    const FMeshNames& Names = GetMeshNames();

    TArray<FVertexTexture> CubeVertices;
    TArray<uint32> CubeIndices;
    FGeometryGenerator::CreateCube(2.0f, 2.0f, 2.0f, CubeVertices, CubeIndices);

    if (!CreateTexturedMesh(Names.Cube, CubeVertices, CubeIndices))
    {
        throw std::runtime_error("TexturedCube mesh creation failed");
    }

    TArray<FVertexTexture> SphereVertices;
    TArray<uint32> SphereIndices;
    // 반지름, 세로로 자르는 개수, 가로로 자르는 개수
    FGeometryGenerator::CreateSphere(1.0f, 64, 32, SphereVertices, SphereIndices);

    if (!CreateTexturedMesh(Names.Sphere, SphereVertices, SphereIndices))
    {
        throw std::runtime_error("TexturedSphere mesh creation failed");
    }

    TArray<FVertexTexture> PlaneVertices;
    TArray<uint32> PlaneIndices;
    FGeometryGenerator::CreatePlane(1.0f, 1.0f, 1, 1, PlaneVertices, PlaneIndices);

    if (!CreateTexturedMesh(Names.Plane, PlaneVertices, PlaneIndices))
    {
        throw std::runtime_error("TexturedPlane mesh creation failed");
    }

    TArray<FVertexTexture> TriangleVertices;
    TArray<uint32> TriangleIndices;
    FGeometryGenerator::CreateTriangle(1.0f, 1.0f, TriangleVertices, TriangleIndices);

    if (!CreateTexturedMesh(Names.Triangle, TriangleVertices, TriangleIndices))
    {
        throw std::runtime_error("TexturedTriangle mesh creation failed");
    }

    // Blender에서 내보낸 UV 포함 로켓 메시
    if (!CreateTexturedMesh("Rocket", rocket_vertices, rocket_indices))
    {
        throw std::runtime_error("Rocket mesh creation failed");
    }

    // SpotLight의 빌보드 아이콘용 정점데이터
    const TArray<FVertexTexture> IconVertices
    {
        { -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f }, //좌하단
        { 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },  //우하단
        { 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f },  //우상단
        { -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }  //좌상단
    };
    const TArray<uint32> IconIndices
    {
        0, 1, 2,
        0, 2, 3
    };
    // 모든 SpotLight가 재사용할 아이콘 메시 등록함
    if (!CreateTexturedMesh(Names.SpotLightIcon, IconVertices, IconIndices))
    {
        throw std::runtime_error("SpotLight icon mesh creation failed");
    }
}

void GResourceManager::RegisterDefaultRenderResources()
{
    const TArray<D3D11_INPUT_ELEMENT_DESC> ColorLayout
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
            0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
            0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
    };

    RegisterShader(FName("Mesh.Color"),L"Assets/Shaders/MainShader.hlsl","mainVS","mainPS",ColorLayout);

    RegisterShader(FName("Editor.Highlight"), L"Assets/Shaders/MainShader.hlsl",
        "VS_Highlight", "PS_Highlight", ColorLayout);
    RegisterShader(FName("Editor.Grid"), L"Assets/Shaders/GridShader.hlsl",
        "VS_Grid", "PS_Grid", ColorLayout);
    RegisterShader(FName("Editor.BatchLine"), L"Assets/Shaders/BatchLineShader.hlsl",
        "mainVS", "mainPS", ColorLayout);

    // 와이어프레임은 메시의 VS를 유지하고 PS만 교체하므로 별도로 소유한다.
    const auto WireframeBlob = CompileResourceShader(
        L"Assets/Shaders/WireframeShader.hlsl", "mainPS", "ps_5_0");
    CheckRenderResourceHR(
        Device->GetDevice()->CreatePixelShader(
            WireframeBlob->GetBufferPointer(), WireframeBlob->GetBufferSize(),
            nullptr, WireframePixelShader.GetAddressOf()),
        "CreateWireframePixelShader");

    const TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
            0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
            0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
            0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0
        },
    };

    RegisterShader(FName("Mesh.Texture"),L"Assets/Shaders/TextureShader.hlsl","mainVS","mainPS",TextureLayout);

    D3D11_SAMPLER_DESC SamplerDesc{};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0.0f;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    RegisterSampler(FName("LinearClamp"), SamplerDesc);

    D3D11_BUFFER_DESC Desc{};
    Desc.ByteWidth = sizeof(FTextureDrawConstants);
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    CheckRenderResourceHR(
        Device->GetDevice()->CreateBuffer(
            &Desc, nullptr, TextureMaterialConstantBuffer.GetAddressOf()),
        "CreateTextureMaterialConstantBuffer");
}
