#include "pch.h"
#include "AssetBrowserWindow.h"

#include "Core/Util/File.h"
#include "Editor/Editor.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/TextureResource.h"

void UAssetBrowserWindow::InitializeWindow(FEditor* InEditor, const FString& InName)
{
	UEditorWindow::InitializeWindow(InEditor, InName);

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
	ImGui::SeparatorText(Relative.string().c_str());

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
				ImGui::Button("Mesh", ImVec2(Size, Size));
			}
			else if (Entry.Type == EAssetType::Texture)
			{
				FTextureResource* Texture = GResourceManager::GetInstance()->GetOrLoadTexture(Path);
				ImGui::ImageButton("Thumbnail", ImTextureRef(Texture->GetSRV()), ImVec2(Size, Size));
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("TEXTURE", Path.c_str(), Path.size() + 1);
					ImGui::Image(ImTextureRef(Texture->GetSRV()), ImVec2(Size, Size));
					ImGui::EndDragDropSource();
				}
			}
			else if (Entry.Type == EAssetType::Directory)
			{
				ImGui::Button("Folder", ImVec2(Size, Size));
			}
			else
			{
				ImGui::BeginDisabled(true);
				ImGui::Button("File", ImVec2(Size, Size));
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
		CurrentContents.Add(AssetEntry);
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
