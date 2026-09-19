#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma once
#include <unordered_map>
#include <map>
#include <string>
#include <span>
#include "Engine/Renderer/Device.h"
#include "Core/Core.h"
#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include "Engine/Renderer/VertexSimple.h"
#include "Engine/Resource/MeshResource.h"
#include "Engine/Renderer/Text/FontAtlas.h"
#include "Core/Container/Map.h"
#include "Core/Name/Name.h"
#include "Engine/Resource/ShaderResource.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Resource/StaticMesh.h"
#include "Engine/Resource/StaticMeshData.h"
#include "Core/Util/Objimporter.h"
#include "Engine/Object/ObjectFactory.h"
#include <wrl/client.h>

class FTextureResource;

class GResourceManager
{
public:
	static GResourceManager* GetInstance();

	void Initialize(GDevice* InDevice);
	void Shutdown();
	FMeshResource* CreateMesh(const FName& MeshName, std::span<const FVertexSimple> Vertices, std::span<const uint32> Indices);
	// 위치와 UV 정점으로 삼각형 메시를 생성하며 실패 시 nullptr을 반환함
	FMeshResource* CreateTexturedMesh(const FName& MeshName, std::span<const FVertexTexture> Vertices, std::span<const uint32> Indices);
	FMeshResource* CreateStaticMeshResource(const FName& MeshName, std::span<const FVertexPNCT> Vertices, std::span<const uint32> Indices);
	
	FMeshResource* GetPrimitive(const FName& MeshName);
	FFontAtlas* GetDefaultFont() { return DefaultFont.GetSRV() ? &DefaultFont : nullptr; }
	FTextureResource* GetOrLoadTexture(const FString& FilePath);
	UStaticMesh* GetOrLoadStaticMesh(const FName& MeshKey);
	UStaticMesh* GetStaticMesh(const FName& Key) { return StaticMeshCache[Key]; }
	UStaticMesh* GetStaticMesh(const FString& FilePath) { return GetStaticMesh(FName(FilePath)); }

	void RegisterShader(const FName& Name,const WCHAR* FilePath,const char* VSEntry,
		const char* PSEntry,const TArray<D3D11_INPUT_ELEMENT_DESC>& Layout);
	void RegisterBlendState(const FName& Name,const D3D11_BLEND_DESC& Desc);
	ID3D11BlendState* GetBlendState(const FName& Name) const;

	const FShaderResource* GetShader(const FName& Name) const;
	ID3D11PixelShader* GetWireframePixelShader() const;

	void RegisterSampler(const FName& Name, const D3D11_SAMPLER_DESC& Desc);

	ID3D11SamplerState* GetSampler(const FName& Name) const;
	FMaterial CreateColorMaterial() const;
	FMaterial CreateTextureMaterial(ID3D11ShaderResourceView* SRV) const;
	FMaterial CreateStaticMeshMaterial(ID3D11ShaderResourceView* SRV) const;
	void RegisterDefaultPrimitives(GDevice* InDevice);
	void RegisterTexturePrimitives(GDevice* InDevice);
	void RegisterRasterizerState(const FName& Name, const D3D11_RASTERIZER_DESC& Desc);
	ID3D11RasterizerState* GetRasterizerState(const FName& Name) const;
	void RegisterDepthStencilState(const FName& Name, const D3D11_DEPTH_STENCIL_DESC& Desc);
	ID3D11DepthStencilState* GetDepthStencilState(const FName& Name) const;


private:
	GResourceManager() = default;
	~GResourceManager() = default;
	GResourceManager(const GResourceManager&) = delete;
	GResourceManager& operator=(const GResourceManager&) = delete;
	void RegisterDefaultRenderResources();
	void RegisterDefaultRasterizerStates();
	void RegisterDefaultBlendStates();
	void RegisterDefaultDepthStencilStates();
	Microsoft::WRL::ComPtr<ID3D11Buffer> TextureMaterialConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> WireframePixelShader;
	TMap<FName, Microsoft::WRL::ComPtr<ID3D11RasterizerState>> RasterizerStateCache;
	TMap<FName, FShaderResource> ShaderCache;
	TMap<FName, Microsoft::WRL::ComPtr<ID3D11SamplerState>> SamplerCache;
	TMap<FName, Microsoft::WRL::ComPtr<ID3D11BlendState>> BlendStateCache;
	TMap<FName, Microsoft::WRL::ComPtr<ID3D11DepthStencilState>> DepthStencilStateCache;
	GDevice* Device = nullptr;

	TMap<FName, FMeshResource*> PrimitiveCache;
	TMap<FString, FTextureResource*> TextureCache;
	TMap<FName, UStaticMesh*> StaticMeshCache;
	FFontAtlas DefaultFont;
};

