#include "pch.h"
#include "OutlinerWindow.h"
#include "Editor/Editor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/ActorComponent.h"
#include "Engine/Component/SceneComponent.h"
#include "Engine/Component/StaticMeshComponent.h"

void UOutlinerWindow::InitializeWindow(FEditor* InEditor, const FString& InName)
{
	UEditorWindow::InitializeWindow(InEditor, InName);
}

void UOutlinerWindow::Render(float DeltaTime)
{
	if (!bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);

	AActor* SelectedActor = Editor->GetSelectedActor();

	ImGui::Begin(Name.c_str(), &bOpen);

	UScene* Scene = Editor->GetCurrentScene();
	if (Scene == nullptr)
	{
		ImGui::End();
		return;
	}

	if (ImGui::BeginTable("ActorTable", 2, ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Visible", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_IndentDisable, ImGui::GetFrameHeight());

		Scene->ForEachActor([this](AActor* Actor)
		{
			if (Actor->GetParentActor() == nullptr)
			{
				DrawActorTree(Actor);
			}
		});
		
		ImGui::EndTable();
	}
	
	ImGui::End();
}

void UOutlinerWindow::DrawActorTree(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);

	const bool bHasChildren = !Actor->GetChildActors().IsEmpty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
	}
	else
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	if (Editor->GetSelectedActor() == Actor)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const FString Label = std::format("{}###Actor{}", Actor->GetName().ToString(), Actor->GetUUID());

	bool bNodeOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		Editor->SetSelectedActor(Actor);
	}

	ImGui::TableSetColumnIndex(1);
	if (Editor->GetSelectedActor() == Actor)
	{
		bool bVisible = Actor->IsVisible();

		ImGui::PushID(Actor);
		if (ImGui::Checkbox("##Visible", &bVisible))
		{
			SetVisibilitySubtree(Actor, bVisible);
		}
		ImGui::PopID();
	}

	ImGui::TableSetColumnIndex(0);

	if (bHasChildren && bNodeOpen)
	{
		for (AActor* Child : Actor->GetChildActors())
		{
			if (Child)
			{
				DrawActorTree(Child);
			}
		}
		ImGui::TreePop();
	}
}

void UOutlinerWindow::SetVisibilitySubtree(AActor* Actor, bool bVisible)
{
		if (!Actor)
	{
		return;
	}

	Actor->SetVisibility(bVisible);

	for (AActor* Child : Actor->GetChildActors())
	{
		if (Child)
		{
			SetVisibilitySubtree(Child, bVisible);
		}
	}
}
