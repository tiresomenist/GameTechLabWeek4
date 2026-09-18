#include <pch.h>
#include "MeshResource.h"

void FMeshResource::AddSection(uint32 InMaterialSlot, uint32 InStartIndex, uint32 InIndexCount)
{
	if (InStartIndex + InIndexCount > IndexCount)
	{
		return;
	}
	Sections.Add(FMeshSection{ InMaterialSlot , InStartIndex, InIndexCount });
}