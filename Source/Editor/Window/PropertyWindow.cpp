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
	USceneComponent* NewComponent = Editor->GetSelectedSceneComponent();
	if (SelectedComponent != NewComponent)
	{
		SelectedComponent = NewComponent;
		bEditingRotation = false;
	}

	if (!SelectedComponent)
	{
		bEditingRotation = false;
		return;
	}
	Translation = SelectedComponent->GetRelativeLocation();
	OScale = SelectedComponent->GetRelativeScale3D();
	// 입력 중에는 창의 임시 값을 유지함
	if (!bEditingRotation)
	{
		RotationDegree = SelectedComponent->GetRelativeRotator();
	}
}

void UPropertyWindow::SetSelectedValue(bool bSetRotation)
{
	if (!SelectedComponent) return;
	SelectedComponent->SetRelativeLocation(Translation);
	if (bSetRotation)
	{
		SelectedComponent->SetRelativeRotation(RotationDegree);
	}
	SelectedComponent->SetRelativeScale3D(OScale);
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
	SelectedComponent = nullptr;
}

void UPropertyWindow::DeleteSelectedActor()
{
	Editor->DeleteSelectedActor();
	SelectedComponent = nullptr;
	bEditingRotation = false;
}

void UPropertyWindow::RenderActorSection(AActor* SelectedActor)
{
	bool bVisible = true;
	for (UActorComponent* Component : SelectedActor->GetComponents())
	{
		if (Component && Component->IsA(UStaticMeshComponent::GetClass()))
		{
			bVisible = static_cast<UStaticMeshComponent*>(Component)->IsVisible();
			break;
		}
	}

	if (ImGui::Checkbox("Visible", &bVisible))
	{
		for (UActorComponent* Component : SelectedActor->GetComponents())
		{
			if (Component && Component->IsA(UStaticMeshComponent::GetClass()))
			{
				static_cast<UStaticMeshComponent*>(Component)->SetVisibility(bVisible);
			}
		}
	}

	const FString ActorName = SelectedActor->GetName().ToString();
	ImGui::Text("Actor: %s", ActorName.c_str());
	if (NameEditingActor != SelectedActor)
	{
		NameEditingActor = SelectedActor;
		ActorNameBuffer.fill('\0');
		const size_t CopyLength = std::min(ActorName.size(), ActorNameBuffer.size() - 1);
		std::copy_n(ActorName.begin(), CopyLength, ActorNameBuffer.begin());
	}

	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputText("Actor Name", ActorNameBuffer.data(), ActorNameBuffer.size(),
		ImGuiInputTextFlags_EnterReturnsTrue) && ActorNameBuffer[0] != '\0')
	{
		SelectedActor->SetName(FName(ActorNameBuffer.data()));
	}
}

void UPropertyWindow::RenderComponentListSection(AActor* SelectedActor)
{
	if (!ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) return;

	for (UActorComponent* Component : SelectedActor->GetComponents())
	{
		const FString ComponentLabel = std::format("{}##{}", Component->GetName().ToString(), Component->GetUUID());
		if (!Component->IsA(USceneComponent::GetClass()))
		{
			ImGui::TextDisabled("%s", ComponentLabel.c_str());
			continue;
		}

		USceneComponent* SceneComponent = static_cast<USceneComponent*>(Component);
		if (ImGui::Selectable(ComponentLabel.c_str(), SelectedComponent == SceneComponent))
		{
			Editor->SetSelectedSceneComponent(SceneComponent);
			GetSelectedValue();
		}
	}
}

void UPropertyWindow::RenderAddComponentSection(AActor* SelectedActor)
{
	if (!ImGui::CollapsingHeader("Add Component##Section", ImGuiTreeNodeFlags_DefaultOpen)) return;

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

	if (!ImGui::Button(bAddingStaticMesh && FindStaticMeshComponent(SelectedActor)
		? "Apply Static Mesh##Action" : "Add Component##Action")) return;

	UActorComponent* AddedComponent = nullptr;
	if (bAddingStaticMesh)
	{
		if (UStaticMeshComponent* StaticMesh = FindStaticMeshComponent(SelectedActor))
		{
			StaticMesh->SetStaticMesh(SelectedMeshKey);
			AddedComponent = StaticMesh;
		}
		else
		{
			AddedComponent = SelectedActor->CreateComponent(SelectedAddComponentClass);
			if (AddedComponent != nullptr)
			{
				static_cast<UStaticMeshComponent*>(AddedComponent)->SetStaticMesh(SelectedMeshKey);
			}
		}
	}
	else
	{
		AddedComponent = SelectedActor->CreateComponent(SelectedAddComponentClass);
	}

	if (AddedComponent != nullptr && AddedComponent->IsA(UPrimitiveComponent::GetClass()))
	{
		EnsurePrimitiveWidget(SelectedActor);
	}
	if (AddedComponent != nullptr && AddedComponent->IsA(USceneComponent::GetClass()))
	{
		Editor->SetSelectedSceneComponent(static_cast<USceneComponent*>(AddedComponent));
	}
}

void UPropertyWindow::RenderActionsSection(AActor* SelectedActor)
{
	if (SelectedComponent != nullptr && SelectedComponent->GetOwner() == SelectedActor)
	{
		if (ImGui::Button("Remove Selected Component"))
		{
			RemoveSelectedComponent();
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("Delete Actor"))
	{
		DeleteSelectedActor();
	}
}

bool UPropertyWindow::RenderTransformSection(bool& bRotationActive)
{
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return false;

	bool bRotationChanged = false;
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("TransformValues", 4, TableFlags))
	{
		ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 78.0f);
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Location");
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##translationX", &Translation.X, SnapSize); DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2); ImGui::DragFloat("##translationY", &Translation.Y, SnapSize); DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3); ImGui::DragFloat("##translationZ", &Translation.Z, SnapSize); DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Rotation");
		ImGui::TableSetColumnIndex(1); bRotationChanged |= DrawRotationField("##rotationR", RotationDegree.Roll, bRotationActive); DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2); bRotationChanged |= DrawRotationField("##rotationP", RotationDegree.Pitch, bRotationActive); DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3); bRotationChanged |= DrawRotationField("##rotationY", RotationDegree.Yaw, bRotationActive); DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Scale");
		ImGui::TableSetColumnIndex(1);
		const FVector BeforeX = OScale;
		float EditedX = OScale.X;
		if (ImGui::DragFloat("##scaleX", &EditedX, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeX, 0, EditedX, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
		ImGui::TableSetColumnIndex(2);
		const FVector BeforeY = OScale;
		float EditedY = OScale.Y;
		if (ImGui::DragFloat("##scaleY", &EditedY, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeY, 1, EditedY, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
		ImGui::TableSetColumnIndex(3);
		const FVector BeforeZ = OScale;
		float EditedZ = OScale.Z;
		if (ImGui::DragFloat("##scaleZ", &EditedZ, 0.001f)) { FVector Result; if (ApplyScaleEdit(BeforeZ, 2, EditedZ, bScaleLock, Result)) OScale = Result; }
		DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);
		ImGui::EndTable();
	}

	char SnapPreview[32];
	snprintf(SnapPreview, sizeof(SnapPreview), "%g", SnapSizeList[SelectedSnapIndex]);
	ImGui::SetNextItemWidth((std::min)(180.0f, ImGui::GetContentRegionAvail().x * 0.55f));
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
	if (SelectedComponent == nullptr) return;

	// 타입별 상세 항목은 여기만 확장한다. 공통 레이아웃과 섞지 않는다.
	if (SelectedComponent->IsA(UFlipbookComponent::GetClass()) &&
		ImGui::CollapsingHeader("SubUV", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* Flame = static_cast<UFlipbookComponent*>(SelectedComponent);
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

	if (SelectedComponent->IsA(UStaticMeshComponent::GetClass()) &&
		ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* MeshComp = static_cast<UStaticMeshComponent*>(SelectedComponent);
		FName NewMeshKey = MeshComp->GetStaticMeshKey();
		if (MeshSelection::DrawCombo("Mesh Key", NewMeshKey)) MeshComp->SetStaticMesh(NewMeshKey);
		std::string CurrentTexPath = MeshComp->GetMaterialPath().c_str();
		if (ImGui::InputText("Texture Path", &CurrentTexPath, ImGuiInputTextFlags_EnterReturnsTrue)) MeshComp->SetMaterial(FString(CurrentTexPath.c_str()));
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			const HWND Owner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
			const auto TexturePath = File::OpenFileDialog(Owner, EFileDialogType::Image, "Assets/Textures");
			if (TexturePath)
			{
				const std::filesystem::path RelativePath = std::filesystem::relative(*TexturePath, std::filesystem::current_path());
				MeshComp->SetMaterial(FString(RelativePath.generic_string().c_str()));
			}
		}
		ImGui::TextDisabled("Type texture path and press Enter.");
	}

	if (SelectedComponent->IsA(USpotLightComponent::GetClass()) &&
		ImGui::CollapsingHeader("SpotLight", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* SpotLight = static_cast<USpotLightComponent*>(SelectedComponent);
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

	if (SelectedComponent->IsA(UTextComponent::GetClass()) &&
		ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* TextComp = static_cast<UTextComponent*>(SelectedComponent);
		FString Text = TextComp->GetText();
		if (ImGui::InputText("Text", &Text)) TextComp->SetText(Text);
	}
}

void UPropertyWindow::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!bOpen) return;

	// 도킹 중에는 DockNode가 크기와 위치를 소유한다. 떠 있을 때만 이 값을 최초 크기로 사용한다.
	ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);
	GetSelectedValue();

	ImGui::Begin(Name.c_str(), &bOpen);
	AActor* SelectedActor = Editor->GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		ImGui::TextDisabled("Select an actor in the viewport or Outliner.");
		ImGui::End();
		return;
	}

	RenderActorSection(SelectedActor);
	ImGui::Separator();
	RenderComponentListSection(SelectedActor);
	RenderAddComponentSection(SelectedActor);
	RenderActionsSection(SelectedActor);

	if (SelectedComponent != nullptr && SelectedComponent->GetOwner() == SelectedActor)
	{
		ImGui::Separator();
		bool bRotationActive = false;
		const bool bRotationChanged = RenderTransformSection(bRotationActive);
		RenderSelectedComponentDetails();
		SetSelectedValue(bRotationChanged);
		bEditingRotation = bRotationActive;
	}

	ImGui::End();
}
