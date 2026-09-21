#include "pch.h"
#include "AssetBrowserWindow.h"

#include "Core/Util/File.h"
#include "Editor/Editor.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/TextureResource.h"

void UAssetBrowserWindow::InitializeWindow(FEditor* InEditor, const FString& InName)
{
	UEditorWindow::InitializeWindow(InEditor, InName);

	DirectoryTexture = GResourceManager::GetInstance()->GetOrLoadTexture("Assets/Editor/Directory.png");
	StaticMeshTexture = GResourceManager::GetInstance()->GetOrLoadTexture("Assets/Editor/StaticMesh.png");
	FileTexture = GResourceManager::GetInstance()->GetOrLoadTexture("Assets/Editor/File.png");

	RefreshDirectoryEntries();
	RefreshCurrentContents();
}

void UAssetBrowserWindow::Render(float DeltaTime)
{
	if (!bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin(Name.c_str(), &bOpen);

	ImGui::BeginChild("Folder Tree", ImVec2(180.0f, 0.0f), ImGuiChildFlags_Borders);

	DrawDirectoryTreeNode(RootDirectoryEntry, true);

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Content View", ImVec2(0.0f, 0.0f));

	DrawContentView();

	ImGui::EndChild();

	ImGui::End();
}

void UAssetBrowserWindow::DrawDirectoryTreeNode(const FDirectoryEntry& Entry, bool bRoot)
{
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (!Entry.Children.IsEmpty())
	{
		Flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	}
	else
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (bRoot)
	{
		Flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	FString Path = File::PathToUtf8(Entry.Path);
	FString Label = File::PathToUtf8(Entry.Path.filename());
	const bool bNodeOpen = ImGui::TreeNodeEx((Label + "###" +  Path).c_str(), Flags);
	if (ImGui::IsItemClicked())
	{
		CurrentPath = Entry.Path;
		RefreshCurrentContents();
	}
	if (bNodeOpen && !Entry.Children.IsEmpty())
	{
		for (const auto& Child : Entry.Children)
		{
			DrawDirectoryTreeNode(Child);
		}
		ImGui::TreePop();
	}
}

void UAssetBrowserWindow::DrawContentView()
{
	const std::filesystem::path Relative = RootPath.filename() / CurrentPath.lexically_relative(RootPath);
	ImGui::SeparatorText(File::PathToUtf8(Relative).c_str());
	
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh"))
	{
		RefreshDirectoryEntries();
		RefreshCurrentContents();
	}

	constexpr float ThumbnailSize = 64.0f;
	constexpr float CellSize = ThumbnailSize + 20.0f;

	const float AvailableRegion = ImGui::GetContentRegionAvail().x;
	const int ColumnCount = std::max(1, static_cast<int>(AvailableRegion / CellSize));

	std::filesystem::path NextPath;
	if (ImGui::BeginTable("AssetGrid", ColumnCount, ImGuiTableFlags_SizingStretchSame))
	{
		for (const auto& Entry : CurrentContents)
		{
			ImGui::TableNextColumn();

			const FString Path = File::PathToUtf8(Entry.Path);
			const FString Label = File::PathToUtf8(Entry.Path.filename());

			ImGui::PushID(Path.c_str());

			const float CellWidth = ImGui::GetContentRegionAvail().x;
			const float Size = std::min(CellWidth, ThumbnailSize);

			if (Entry.Type == EAssetType::StaticMesh)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton("StaticMesh", ImTextureRef(StaticMeshTexture->GetSRV()), ImVec2(Size, Size));
				ImGui::PopStyleColor();
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("STATIC_MESH", Path.c_str(), Path.size() + 1);
					ImGui::Image(ImTextureRef(StaticMeshTexture->GetSRV()), ImVec2(Size, Size));
					ImGui::EndDragDropSource();
				}
			}
			else if (Entry.Type == EAssetType::Texture)
			{
				FTextureResource* Texture = GResourceManager::GetInstance()->GetOrLoadTexture(Path);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton("Thumbnail", ImTextureRef(Texture->GetSRV()), ImVec2(Size, Size));
				ImGui::PopStyleColor();
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("TEXTURE", Path.c_str(), Path.size() + 1);
					ImGui::Image(ImTextureRef(Texture->GetSRV()), ImVec2(Size, Size));
					ImGui::EndDragDropSource();
				}
			}
			else if (Entry.Type == EAssetType::Directory)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton("Directory", ImTextureRef(DirectoryTexture->GetSRV()), ImVec2(Size, Size));
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::BeginDisabled(true);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton("File", ImTextureRef(FileTexture->GetSRV()), ImVec2(Size, Size));
				ImGui::PopStyleColor();
				ImGui::EndDisabled();
			}

			const bool bHovered = ImGui::IsItemHovered();
			if (Entry.Type == EAssetType::Directory && 
				bHovered && 
				ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				NextPath = Entry.Path;
			}

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + CellWidth);
			ImGui::TextUnformatted(Label.c_str());
			ImGui::PopTextWrapPos();

			ImGui::Spacing();

			ImGui::PopID();
		}
		
		ImGui::EndTable();
	}

	if (!NextPath.empty())
	{
		CurrentPath = NextPath;
		RefreshCurrentContents();
	}
}

void UAssetBrowserWindow::RefreshDirectoryEntries()
{
	if (RootPath.empty() ||
		!std::filesystem::exists(RootPath) ||
		!std::filesystem::is_directory(RootPath))
	{
		RootDirectoryEntry = {};
		return;
	}

	RootDirectoryEntry = ConstructDirectoryEntry(RootPath);
}

void UAssetBrowserWindow::RefreshCurrentContents()
{
	CurrentContents.Empty();

	if (CurrentPath.empty() ||
		!std::filesystem::exists(CurrentPath) ||
		!std::filesystem::is_directory(CurrentPath))
	{
		return;
	}

	TArray<FAssetEntry> DirectoryEntries;
	TArray<FAssetEntry> FileEntries;
	for (const auto& Entry : std::filesystem::directory_iterator(CurrentPath))
	{
		FAssetEntry AssetEntry{
			.Path = Entry.path(),
			.Type = Entry.is_directory() ? EAssetType::Directory : EAssetType::Unknown,
		};
		if (Entry.is_regular_file())
		{
			const FString Extension = File::PathToUtf8(Entry.path().extension());
			if (Extension == ".obj")
			{
				AssetEntry.Type = EAssetType::StaticMesh;
			}
			else if (Extension == ".png")
			{
				AssetEntry.Type = EAssetType::Texture;
			}
		}

		if (AssetEntry.Type == EAssetType::Directory)
		{
			DirectoryEntries.Add(AssetEntry);
		}
		else
		{
			FileEntries.Add(AssetEntry);
		}
	}

	for (const auto& Entry : DirectoryEntries)
	{
		CurrentContents.Add(Entry);
	}
	for (const auto& Entry : FileEntries)
	{
		CurrentContents.Add(Entry);
	}
}

FDirectoryEntry UAssetBrowserWindow::ConstructDirectoryEntry(const std::filesystem::path& Path) const
{
	FDirectoryEntry Result{
		.Path = Path,
	};
	if (!std::filesystem::is_directory(Path))
	{
		return Result;
	}
	for (const auto& Entry : std::filesystem::directory_iterator(Path))
	{
		if (Entry.is_directory())
		{
			Result.Children.Add(ConstructDirectoryEntry(Entry.path()));
		}
	}
	return Result;
}
