#pragma once
#pragma once
#include "Core/Math/Rect.h"
#include "Core/Math/Point.h"
#include "Engine/Renderer/ViewportClient.h"

class SWindow
{
public:
	bool IsHover(const FPoint& coord) const;
	virtual void UpdateLayout(const FRect& InRect);

	void SetViewportClient(FViewportClient* InViewportClient);
		
protected:
	FRect Rect;
	FViewportClient* ViewportClient = nullptr;
};

