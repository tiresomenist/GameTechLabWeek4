#pragma once

#include "EditorWindow.h"
#include <filesystem>

class FTextureResource;

enum class EAssetType
{
	Directory,
	StaticMesh,
	Texture,
	Unknown,
};

struct FDirectoryEntry
{
	std::filesystem::path Path;
	TArray<FDirectoryEntry> Children;
};

struct FAssetEntry
{
	std::filesystem::path Path;
	EAssetType Type;
};

class UAssetBrowserWindow : public UEditorWindow
{
	UCLASS(UAssetBrowserWindow, "AssetBrowserWindow", UEditorWindow)

public:
	virtual void InitializeWindow(FEditor* InEditor, const FString& InName) override;
	virtual void Render(float DeltaTime) override;

private:
	void DrawDirectoryTreeNode(const FDirectoryEntry& Entry, bool bRoot = false);
	void DrawContentView();

	void RefreshDirectoryEntries();
	void RefreshCurrentContents();
	FDirectoryEntry ConstructDirectoryEntry(const std::filesystem::path& Path) const;

	std::filesystem::path RootPath = "Assets";
	std::filesystem::path CurrentPath = "Assets";

	FDirectoryEntry RootDirectoryEntry;
	TArray<FAssetEntry> CurrentContents;

	FTextureResource* DirectoryTexture = nullptr;
	FTextureResource* StaticMeshTexture = nullptr;
	FTextureResource* FileTexture = nullptr;
};
