#include "pch.h"
#include "OutlinerWindow.h"
#include "Editor/Editor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/ActorComponent.h"
#include "Engine/Component/SceneComponent.h"

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

	Scene->ForEachActor([this](AActor* Actor)
		{
			if (Actor->GetParentActor() == nullptr)
			{
				DrawActorTree(Actor);
			}
		});

	ImGui::End();
}

void UOutlinerWindow::DrawActorTree(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	const bool bHasChildren = !Actor->GetChildActors().IsEmpty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth;
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
