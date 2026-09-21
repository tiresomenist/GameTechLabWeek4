#include "pch.h"
#include "Editor/Util/ScaleEdit.h"
#include "PropertyWindow.h"
#include "Core/Math/Vector.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#include "Editor/Editor.h"
#include "Core/Math/Quaternion.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Primitive/PrimitiveComponent.h"
#include "Engine/Component/Primitive/FlipbookComponent.h"
#include "Engine/Component/Primitive/TextComponent.h"
#include "Engine/Component/MeshComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Component/WidgetComponent.h"
#include "Core/Math/Rotator.h"
#include "Core/Util/File.h"
#include "Engine/Component/Light/SpotLightComponent.h"
#include "Editor/Util/MeshSelection.h"

namespace
{
	UStaticMeshComponent* FindStaticMeshComponent(AActor* Actor)
	{
		if (Actor == nullptr) return nullptr;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component->IsA(UStaticMeshComponent::GetClass()))
			{
				return static_cast<UStaticMeshComponent*>(Component);
			}
		}

		return nullptr;
	}

	void EnsurePrimitiveWidget(AActor* Actor)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component->IsA(UWidgetComponent::GetClass())) return;
		}

		Actor->CreateComponent(UWidgetComponent::GetClass());
	}
}

void UPropertyWindow::InitializeWindow(FEditor* InEditor, const FString& Name)
{
	UEditorWindow::InitializeWindow(InEditor, Name);

	AddableComponentClasses.Add(UStaticMeshComponent::GetClass());
	AddableComponentClasses.Add(UTextComponent::GetClass());
	AddableComponentClasses.Add(UFlipbookComponent::GetClass());
	AddableComponentClasses.Add(USpotLightComponent::GetClass());
	SelectedAddComponentClass = *AddableComponentClasses.begin();

	SelectedMeshKey = MeshSelection::GetEntries()[0].Key;
}

void UPropertyWindow::GetSelectedValue()
{
	USceneComponent* NewComponent = Editor->GetTransformTarget();
	if (TransformTarget != NewComponent)
	{
		TransformTarget = NewComponent;
		bEditingRotation = false;
	}

	InspectedComponent = Editor->GetSelectedComponent();
	if (!InspectedComponent)
	{
		InspectedComponent = TransformTarget;
	}

	if (!TransformTarget)
	{
		bEditingRotation = false;
		return;
	}
	Translation = TransformTarget->GetRelativeLocation();
	OScale = TransformTarget->GetRelativeScale3D();
	// 입력 중에는 창의 임시 값을 유지함
	if (!bEditingRotation)
	{
		RotationDegree = TransformTarget->GetRelativeRotator();
	}
}

void UPropertyWindow::SetSelectedValue(bool bSetRotation)
{
	if (!TransformTarget) return;
	TransformTarget->SetRelativeLocation(Translation);
	if (bSetRotation)
	{
		TransformTarget->SetRelativeRotation(RotationDegree);
	}
	TransformTarget->SetRelativeScale3D(OScale);
}

bool UPropertyWindow::DrawRotationField(const char* ID, float& Degree, bool& bRotationActive)
{
	const float PreviousDegree = Degree;
	const bool bChanged = ImGui::DragFloat(ID, &Degree, 0.1f, 0.0f, 0.0f, "%.3f");

	bRotationActive |= ImGui::IsItemActive();
	if (bChanged && !std::isfinite(Degree))
	{
		Degree = PreviousDegree;
		return false;
	}
	return bChanged;
}


void UPropertyWindow::RemoveSelectedComponent()
{
	Editor->RemoveSelectedComponent();
	InspectedComponent = nullptr;
	TransformTarget = nullptr;
	bEditingRotation = false;
}

void UPropertyWindow::DeleteSelectedActor()
{
	Editor->DeleteSelectedActor();
	InspectedComponent = nullptr;
	TransformTarget = nullptr;
	bEditingRotation = false;
}

void UPropertyWindow::RenderComponentTreeSection(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (!ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (ImGui::BeginChild("ComponentTree", ImVec2(0.0f, 200.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
	{
		const bool bCanUseShortcuts =
			ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
			RenameTarget == nullptr &&
			!ImGui::IsAnyItemActive() &&
			!ImGui::GetIO().WantTextInput &&
			!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup | ImGuiPopupFlags_AnyPopupLevel);

		if (bCanUseShortcuts)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
			{
				RequestRename(Editor->GetSelectedComponent());
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			{
				PendingDeleteTarget = Editor->GetSelectedComponent();
			}
		}

		USceneComponent* Root = Actor->GetRootComponent();
		DrawComponentTree(Root);

		// Root에 연결되지 않은 SceneComponent
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || !Component->IsA(USceneComponent::GetClass()))
			{
				continue;
			}

			USceneComponent* SceneComponent = static_cast<USceneComponent*>(Component);
			if (SceneComponent != Root && SceneComponent->GetAttachParent() == nullptr)
			{
				DrawComponentTree(SceneComponent);
			}
		}

		// Transform 계층 구조가 없는 SceneComponent
		bool bHasOtherComponents = false;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || Component->IsA(USceneComponent::GetClass()))
			{
				continue;
			}

			if (!bHasOtherComponents)
			{
				ImGui::SeparatorText("Other Components");
				bHasOtherComponents = true;
			}

			const FString Label = std::format("{}###Component{}", Component->GetName().ToString(), Component->GetUUID());

			if (RenameTarget == Component)
			{
				const ImVec2 InputPosition = ImGui::GetCursorScreenPos();
				const float InputWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);

				DrawRenameInput(Component, InputPosition, InputWidth);
			}
			else
			{
				if (ImGui::Selectable(Label.c_str(), Editor->GetSelectedComponent() == Component))
				{
					Editor->SetSelectedComponent(Component);
					GetSelectedValue();
				}
				DrawComponentContextMenu(Component);
			}
		}
	}

	ImGui::EndChild();
}

void UPropertyWindow::DrawComponentTree(USceneComponent* Component)
{
	if (Component == nullptr)
	{
		return;
	}

	const bool bRenaming = RenameTarget == Component;
	const bool bHasChildren = !Component->GetAttachChildren().IsEmpty();
	const bool bIsRoot = Component == Component->GetOwner()->GetRootComponent();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
	}
	else
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (bRenaming)
	{
		Flags &= ~ImGuiTreeNodeFlags_SpanAvailWidth;
		Flags |= ImGuiTreeNodeFlags_AllowOverlap;
	}

	if (Editor->GetSelectedSceneComponent() == Component)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const FString DisplayLabel = std::format("{}{}", 
		bRenaming ? "" : Component->GetName().ToString(),
		!bRenaming && bIsRoot ? " (Root)" : "");
	const FString Label = std::format("{}###Component{}", DisplayLabel, Component->GetUUID());

	const ImVec2 RowStart = ImGui::GetCursorScreenPos();
	const float LabelSpacing = ImGui::GetTreeNodeToLabelSpacing();

	const ImVec2 InputPosition(RowStart.x + LabelSpacing, RowStart.y);
	const float InputWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - LabelSpacing);

	const bool bNodeOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	if (!bIsRoot && ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("EDITOR_SCENE_COMPONENT", &Component, sizeof(Component));
		ImGui::TextUnformatted(Component->GetName().ToString().c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("EDITOR_SCENE_COMPONENT"))
		{
			USceneComponent* DraggedComponent = *static_cast<USceneComponent**>(Payload->Data);
			if (CanReparent(DraggedComponent, Component))
			{
				PendingReparentSource = DraggedComponent;
				PendingReparentTarget = Component;
			}
		}
		ImGui::EndDragDropTarget();
	}
	
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		Editor->SetSelectedComponent(Component);
		GetSelectedValue();
	}

	DrawComponentContextMenu(Component);

	if (bRenaming)
	{
		ImGui::SameLine();
		DrawRenameInput(Component, InputPosition, InputWidth);
	}

	if (bHasChildren && bNodeOpen)
	{
		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			DrawComponentTree(Child);
		}

		ImGui::TreePop();
	}

}

void UPropertyWindow::RenderAddComponentSection(AActor* Actor)
{
	if (!ImGui::CollapsingHeader("Add Component##Section")) return;

	if (ImGui::BeginCombo("Component Type", SelectedAddComponentClass->DisplayName.c_str()))
	{
		for (FClassType* ComponentClass : AddableComponentClasses)
		{
			const bool bSelected = SelectedAddComponentClass == ComponentClass;
			if (ImGui::Selectable(ComponentClass->DisplayName.c_str(), bSelected))
			{
				SelectedAddComponentClass = ComponentClass;
			}
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const bool bAddingStaticMesh = SelectedAddComponentClass == UStaticMeshComponent::GetClass();
	if (bAddingStaticMesh)
	{
		MeshSelection::DrawCombo("Mesh", SelectedMeshKey);
	}

	if (!ImGui::Button(bAddingStaticMesh && FindStaticMeshComponent(Actor)
		? "Apply Static Mesh##Action" : "Add Component##Action")) return;

	UActorComponent* AddedComponent = nullptr;
	if (bAddingStaticMesh)
	{
		if (UStaticMeshComponent* StaticMesh = FindStaticMeshComponent(Actor))
		{
			StaticMesh->SetStaticMesh(SelectedMeshKey);
			AddedComponent = StaticMesh;
		}
		else
		{
			AddedComponent = Actor->CreateComponent(SelectedAddComponentClass);
			if (AddedComponent != nullptr)
			{
				static_cast<UStaticMeshComponent*>(AddedComponent)->SetStaticMesh(SelectedMeshKey);
			}
		}
	}
	else
	{
		AddedComponent = Actor->CreateComponent(SelectedAddComponentClass);
	}

	if (AddedComponent != nullptr && AddedComponent->IsA(UPrimitiveComponent::GetClass()))
	{
		EnsurePrimitiveWidget(Actor);
	}
	if (AddedComponent != nullptr)
	{
		Editor->SetSelectedComponent(AddedComponent);
		GetSelectedValue();
	}
}

bool UPropertyWindow::RenderTransformSection(bool& bRotationActive)
{
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return false;

	bool bRotationChanged = false;
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("TransformValues", 4, TableFlags))
	{
		ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 78.0f);
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Location");
		ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::DragFloat("##translationX", &Translation.X, SnapSize); DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::DragFloat("##translationY", &Translation.Y, SnapSize); DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::DragFloat("##translationZ", &Translation.Z, SnapSize); DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Rotation");
		ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); bRotationChanged |= DrawRotationField("##rotationR", RotationDegree.Roll, bRotationActive); DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); bRotationChanged |= DrawRotationField("##rotationP", RotationDegree.Pitch, bRotationActive); DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); bRotationChanged |= DrawRotationField("##rotationY", RotationDegree.Yaw, bRotationActive); DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Scale");
		ImGui::TableSetColumnIndex(1);
		const FVector BeforeX = OScale;
		float EditedX = OScale.X;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat("##scaleX", &EditedX, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeX, 0, EditedX, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2);
		const FVector BeforeY = OScale;
		float EditedY = OScale.Y;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat("##scaleY", &EditedY, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeY, 1, EditedY, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3);
		const FVector BeforeZ = OScale;
		float EditedZ = OScale.Z;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat("##scaleZ", &EditedZ, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeZ, 2, EditedZ, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);
		ImGui::EndTable();
	}

	char SnapPreview[32];
	snprintf(SnapPreview, sizeof(SnapPreview), "%g", SnapSizeList[SelectedSnapIndex]);
	ImGui::SetNextItemWidth((std::min)(80.0f, ImGui::GetContentRegionAvail().x * 0.55f));
	if (ImGui::BeginCombo("Snap Size", SnapPreview))
	{
		for (int32 Index = 0; Index < SnapSizeList.Num(); ++Index)
		{
			const bool bSelected = SelectedSnapIndex == Index;
			char ItemName[32];
			snprintf(ItemName, sizeof(ItemName), "%g", SnapSizeList[Index]);
			if (ImGui::Selectable(ItemName, bSelected)) SelectedSnapIndex = Index;
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	SnapSize = SnapSizeList[SelectedSnapIndex];
	ImGui::SameLine();
	ImGui::Checkbox("Scale Lock", &bScaleLock);
	return bRotationChanged;
}

void UPropertyWindow::RenderSelectedComponentDetails()
{
	if (InspectedComponent == nullptr) return;

	// 타입별 상세 항목은 여기만 확장한다. 공통 레이아웃과 섞지 않는다.
	if (InspectedComponent->IsA(UFlipbookComponent::GetClass()) &&
		ImGui::CollapsingHeader("SubUV", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* Flame = static_cast<UFlipbookComponent*>(InspectedComponent);
		int Grid[2] = { Flame->GetColumns(), Flame->GetRows() };
		if (ImGui::InputInt2("Columns / Rows", Grid)) Flame->SetAtlasGrid(Grid[0], Grid[1]);
		int FrameCount = Flame->GetFrameCount();
		if (ImGui::InputInt("Frame Count", &FrameCount)) Flame->SetAtlasGrid(Flame->GetColumns(), Flame->GetRows(), FrameCount);
		float FPS = Flame->GetFramesPerSecond();
		if (ImGui::DragFloat("FPS", &FPS, 1.0f, 0.0f, 240.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) Flame->SetFramesPerSecond(FPS);
		float Rate = Flame->GetPlayRate();
		if (ImGui::DragFloat("Play Rate", &Rate, 0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) Flame->SetPlayRate(Rate);
		bool bLoop = Flame->IsLooping();
		if (ImGui::Checkbox("Loop", &bLoop)) Flame->SetLooping(bLoop);
		ImGui::SameLine();
		bool bPlaying = Flame->IsPlaying();
		if (ImGui::Checkbox("Playing", &bPlaying)) Flame->SetPlaying(bPlaying);
		ImGui::SameLine();
		if (ImGui::Button("Restart")) Flame->Restart();
		int Frame = Flame->GetCurrentFrame();
		if (ImGui::SliderInt("Frame", &Frame, 0, Flame->GetFrameCount() - 1)) { Flame->SetCurrentFrame(Frame); Flame->SetPlaying(false); }
	}

	if (InspectedComponent->IsA(UStaticMeshComponent::GetClass()) &&
		ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* MeshComp = static_cast<UStaticMeshComponent*>(InspectedComponent);
		FName NewMeshKey = MeshComp->GetStaticMeshKey();
		ImGui::SetNextItemWidth(150.0f);
		if (MeshSelection::DrawCombo("Mesh Key", NewMeshKey)) MeshComp->SetStaticMesh(NewMeshKey);
		UStaticMesh* Mesh = MeshComp->GetStaticMesh();
		uint32 NumSlots = Mesh ? static_cast<uint32>(Mesh->GetDefaultMeshMaterials().Num()) : 0;

		if (NumSlots == 0)
		{
			ImGui::TextDisabled("No material slots available.");
		}
		else
		{
			ImGui::SeparatorText("Material Slots");
			for (uint32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
			{
				ImGui::PushID(static_cast<int>(SlotIdx));
				std::string CurrentTexPath = MeshComp->GetMaterialPath(SlotIdx).c_str();

				ImGui::Text("Slot [%u]", SlotIdx);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::InputText("##TexturePath", &CurrentTexPath, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					MeshComp->SetOverrideMaterial(FString(CurrentTexPath.c_str()), SlotIdx);
				}
				ImGui::SameLine();
				if (ImGui::Button("Browse..."))
				{
					const HWND Owner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
					const auto TexturePath = File::OpenFileDialog(Owner, EFileDialogType::Image, "Assets/Textures");
					if (TexturePath)
					{
						const std::filesystem::path RelativePath =std::filesystem::relative(*TexturePath, std::filesystem::current_path());
						MeshComp->SetOverrideMaterial(File::PathToUtf8(RelativePath), SlotIdx);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					MeshComp->SetOverrideMaterial("", SlotIdx);
				}
				ImGui::PopID();
			}
			ImGui::TextDisabled("Press Enter to apply path, or Reset to default.");
		}
	}

	if (InspectedComponent->IsA(UMeshComponent::GetClass()) &&
		ImGui::CollapsingHeader("UV & Scroll", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* MeshComp = static_cast<UMeshComponent*>(InspectedComponent);

		bool bScroll = MeshComp->IsUVScrollEnabled();
		if (ImGui::Checkbox("Enable UV Scroll", &bScroll))
		{
			MeshComp->SetUVScrollEnabled(bScroll);
		}

		FVector2 Scale = MeshComp->GetUVScale();
		if (ImGui::DragFloat2("UV Scale", &Scale.X, 0.05f, 0.01f, 50.0f, "%.2f"))
		{
			MeshComp->SetUVScale(Scale);
		}

		FVector2 Speed = MeshComp->GetScrollSpeed();
		if (ImGui::DragFloat2("Scroll Speed", &Speed.X, 0.01f, -10.0f, 10.0f, "%.3f"))
		{
			MeshComp->SetScrollSpeed(Speed);
		}

		const FVector2& Offset = MeshComp->GetUVOffset();
		ImGui::Text("Current Offset: (%.3f, %.3f)", Offset.X, Offset.Y);
		ImGui::SameLine();
		if (ImGui::Button("Reset Offset"))
		{
			MeshComp->ResetUVOffset();
		}
	}

	if (InspectedComponent->IsA(USpotLightComponent::GetClass()) &&
		ImGui::CollapsingHeader("SpotLight", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* SpotLight = static_cast<USpotLightComponent*>(InspectedComponent);
		ImGui::PushID(SpotLight);
		const FVector& CurrentColor = SpotLight->GetLightColor();
		float Color[3] { CurrentColor.X, CurrentColor.Y, CurrentColor.Z };
		const ImGuiColorEditFlags PickerFlags = ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
			ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8 |
			ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions;
		ImGui::TextUnformatted("Light Color");
		ImGui::SetNextItemWidth((std::min)(ImGui::GetContentRegionAvail().x, 300.0f));
		if (ImGui::ColorPicker3("##LightColor", Color, PickerFlags)) SpotLight->SetLightColor(FVector(Color[0], Color[1], Color[2]));
		ImGui::Separator();
		float Radius = SpotLight->GetConeRadius();
		if (ImGui::DragFloat("Cone Radius", &Radius, 0.05f, 0.0f, 0.0f, "%.2f")) SpotLight->SetConeRadius(Radius);
		float Distance = SpotLight->GetConeLength();
		if (ImGui::DragFloat("Cone Distance", &Distance, 0.05f, 0.0f, 0.0f, "%.2f")) SpotLight->SetConeLength(Distance);
		ImGui::PopID();
	}

	if (InspectedComponent->IsA(UTextComponent::GetClass()) &&
		ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* TextComp = static_cast<UTextComponent*>(InspectedComponent);
		FString Text = TextComp->GetText();
		if (ImGui::InputText("Text", &Text)) TextComp->SetText(Text);
	}
}

void UPropertyWindow::Render(float DeltaTime)
{
	bRenameInputDrawn = false;

	if (!bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);
	GetSelectedValue();

	ImGui::Begin(Name.c_str(), &bOpen);
	AActor* SelectedActor = Editor->GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		ImGui::End();
		return;
	}

	ImGui::SeparatorText(SelectedActor->GetName().ToString().c_str());
	RenderAddComponentSection(SelectedActor);
	RenderComponentTreeSection(SelectedActor);

	if (USceneComponent* Component = Editor->GetSelectedSceneComponent())
	{
		bool bVisible = Component->GetVisibility();
		if (ImGui::Checkbox("Visible", &bVisible))
		{
			Component->SetVisibility(bVisible);
		}

		if (bVisible && !Component->IsVisible())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Hidden by Actor");
		}
	}

	if (TransformTarget)
	{
		ImGui::Separator();
		bool bRotationActive = false;
		const bool bRotationChanged = RenderTransformSection(bRotationActive);

		SetSelectedValue(bRotationChanged);
		bEditingRotation = bRotationActive;
	}

	if (InspectedComponent)
	{
		RenderSelectedComponentDetails();
	}

	if (PendingDeleteTarget)
	{
		Editor->SetSelectedComponent(PendingDeleteTarget);
		RemoveSelectedComponent();

		PendingDeleteTarget = nullptr;
		FinishRename(false);
	}
	else if (RenameTarget && !bFocusRenameInput && !bRenameInputDrawn)
	{
		FinishRename(true);
	}

	if (PendingReparentSource && PendingReparentTarget)
	{
		PendingReparentSource->AttachTo(PendingReparentTarget);
		PendingReparentSource = nullptr;
		PendingReparentTarget = nullptr;
	}

	ImGui::End();
}

void UPropertyWindow::DrawComponentContextMenu(UActorComponent* Component)
{
	if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::IsWindowAppearing())
		{
			Editor->SetSelectedComponent(Component);
			GetSelectedValue();
		}

		if (ImGui::MenuItem("Rename", "F2"))
		{
			RequestRename(Component);
		}

		if (ImGui::MenuItem("Delete", "Delete"))
		{
			PendingDeleteTarget = Component;
		}

		ImGui::EndPopup();
	}
}

void UPropertyWindow::DrawRenameInput(UActorComponent* Component, const ImVec2& Position, float Width)
{
	ImGui::SetCursorScreenPos(Position);
	ImGui::SetNextItemWidth(Width);

	ImGui::PushID(Component);

	if (bFocusRenameInput)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusRenameInput = false;
	}

	const bool bEscapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
	const bool bEnter = ImGui::InputText("##Rename", &RenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	if (bEscapePressed && (ImGui::IsItemActive() || ImGui::IsItemDeactivated()))
	{
		FinishRename(false);
	}
	else if (bEnter || ImGui::IsItemDeactivated())
	{
		FinishRename(true);
	}

	ImGui::PopID();

	bRenameInputDrawn = true;
}

void UPropertyWindow::RequestRename(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	RenameTarget = Component;

	const FString CurrentName = Component->GetName().ToString();
	RenameBuffer = CurrentName;
	bFocusRenameInput = true;
}

void UPropertyWindow::FinishRename(bool bApply)
{
	if (bApply && RenameTarget && !RenameBuffer.empty())
	{
		RenameTarget->SetName(FName(RenameBuffer));
	}

	RenameTarget = nullptr;
	bFocusRenameInput = false;
	RenameBuffer.clear();
}

bool UPropertyWindow::CanReparent(USceneComponent* Source, USceneComponent* Target) const
{
	if (!Source || !Target)
	{
		return false;
	}
	if (Source->GetOwner() != Target->GetOwner())
	{
		return false;
	}

	for (USceneComponent* Parent = Target; Parent != nullptr; Parent = Parent->GetAttachParent())
	{
		if (Parent == Source)
		{
			return false;
		}
	}

	return true;
}
