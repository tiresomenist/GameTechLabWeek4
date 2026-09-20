#include "pch.h"
#include "Window.h"

bool SWindow::IsHover(const FPoint& coord) const
{

    return Rect.Contains(coord);
}

void SWindow::UpdateLayout(const FRect& InRect)
{
    Rect = InRect;
    if (ViewportClient)
    {
        ViewportClient->SetRect(Rect.X, Rect.Y, Rect.Width, Rect.Height);
    }
}

void SWindow::SetViewportClient(FViewportClient* InViewportClient)
{
    ViewportClient = InViewportClient;
}
