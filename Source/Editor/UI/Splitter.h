#pragma once
#include "Window.h"

class SSplitter :
    public SWindow
{
public:
    //virtual void UpdateLayout();

    SWindow* SideLT;    // Left or Top
    SWindow* SideRB;    // Right or Bottom
    
protected:
    float SplitRatio = 0.5f;        // LT:RB의 분할 비율
    float SplitterThickness = 3.0f; // 분할된 선의 두께
};

// 수평배치 (좌우)
class SSplitterH : public SSplitter
{
public:
    virtual void UpdateLayout(const FRect& InRect) override
    {
        Rect = InRect;

        float HalfThick = SplitterThickness * 0.5f;
        float SplitX = InRect.X + (InRect.Width * SplitRatio);

        // 좌측
        FRect LeftRect{ InRect.X, InRect.Y, (SplitX - HalfThick) - InRect.X, InRect.Height };
        FRect RightRect{ SplitX + HalfThick, InRect.Y, InRect.Right() - (SplitX + HalfThick), InRect.Height};

        if (SideLT)
        {
            SideLT->UpdateLayout(LeftRect);
        }

        if (SideRB)
        {
            SideRB->UpdateLayout(RightRect);
        }
    }

    void SetLeft(SWindow* InLeft) { SideLT = InLeft; }
    void SetRight(SWindow* InRight) { SideRB = InRight; }
};

// 수직배치 (상하)
class SSplitterV : public SSplitter
{
public:
    virtual void UpdateLayout(const FRect& InRect) override
    {
        Rect = InRect;

        float HalfThick = SplitterThickness * 0.5f;
        float SplitY = InRect.Y + (InRect.Height * SplitRatio);

        FRect TopRect{ InRect.X, InRect.Y, InRect.Width, (SplitY - HalfThick) - InRect.Y };
        FRect BotRect{ InRect.X, SplitY + HalfThick, InRect.Width, InRect.Bottom() - (SplitY+ HalfThick)};

        if (SideLT)
        {
            SideLT->UpdateLayout(TopRect);
        }

        if (SideRB)
        {
            SideRB->UpdateLayout(BotRect);
        }
    }

    void SetTop(SWindow* InTop) { SideLT = InTop; }
    void SetBottom(SWindow* InBot) { SideRB = InBot; }

};