#pragma once
#include "Window.h"

class SSplitter :
    public SWindow
{
public:
    //virtual void UpdateLayout();

    SWindow* SideLT;    // Left or Top
    SWindow* SideRB;    // Right or Bottom

private:

    float SplitRatio = 0.5f;        // LT:RB의 분할 비율
    float SplitterThickness = 3.0f; // 분할된 선의 두께
};


class SSplitterH : public SSplitter
{
public:

};


class SSplitterV : public SSplitter
{
public:

};