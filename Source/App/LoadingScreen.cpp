#include "pch.h"
#include "LoadingScreen.h"
#include "resource.h"
#include <exception>
#include <stdexcept>
#include <utility>

// 소멸 전에 실행 중인 UI 스레드를 정리한다.
FLoadingScreen::~FLoadingScreen()
{
    Stop();
}

// UI 스레드를 시작하고 로고 창이 준비될 때까지 기다린다.
void FLoadingScreen::Start(HINSTANCE Instance, FWideStringView Title)
{
    if (UIThread.joinable())
        throw std::logic_error("Loading screen is already running.");

    // 스레드 시작 전에 공유할 초기 상태와 종료 이벤트를 준비한다.
    ApplicationTitle = FWideString(Title);
    bCancelRequested.store(false);
    StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!StopEvent)
        throw std::runtime_error("Failed to create loading screen stop event.");

    try
    {
        // 성공과 예외를 모두 전달받아 창 생성 실패 시 대기가 끝나도록 한다.
        std::promise<void> Startup;
        std::future<void> Ready = Startup.get_future();
        UIThread = std::thread(&FLoadingScreen::ThreadMain, this, Instance, std::move(Startup));
        Ready.get();
    }
    catch (...)
    {
        Stop();
        throw;
    }
}

// 종료 이벤트를 전달하고 UI 스레드가 창을 정리할 때까지 기다린다.
void FLoadingScreen::Stop() noexcept
{
    // 호출자는 창을 직접 파괴하지 않고 소유 스레드에 종료를 알린다.
    if (StopEvent) SetEvent(StopEvent);
    if (UIThread.joinable()) UIThread.join();

    if (StopEvent)
    {
        CloseHandle(StopEvent);
        StopEvent = nullptr;
    }
}

// 사용자가 요청한 취소 상태를 스레드 간 안전하게 읽는다.
bool FLoadingScreen::IsCancelRequested() const noexcept
{
    return bCancelRequested.load();
}

// 별도 UI 스레드에서 로딩 창의 전체 수명을 관리한다.
void FLoadingScreen::ThreadMain(HINSTANCE Instance, std::promise<void> Startup)
{
    constexpr wchar_t ClassName[] = L"PepeLoadingScreenWindow";
    HWND Window = nullptr;
    bool bClassRegistered = false;
    bool bStartupReported = false;

    try
    {
        // 실행 파일에 포함된 아이콘을 로고 크기로 읽는다.
        LogoIcon = static_cast<HICON>(LoadImageW(
            Instance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 96, 96, LR_DEFAULTCOLOR));
        if (!LogoIcon)
            throw std::runtime_error("Failed to load PePe loading screen icon.");

        WNDCLASSW WindowClass{};
        WindowClass.lpfnWndProc = &FLoadingScreen::WindowProc;
        WindowClass.hInstance = Instance;
        WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        WindowClass.hIcon = LogoIcon;
        WindowClass.lpszClassName = ClassName;
        if (!RegisterClassW(&WindowClass))
            throw std::runtime_error("Failed to register loading screen window.");
        bClassRegistered = true;

        // 클라이언트 영역을 480×300으로 맞추고 화면 중앙에 배치한다.
        constexpr DWORD Style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
        RECT Bounds{ 0, 0, 480, 300 };
        if (!AdjustWindowRectEx(&Bounds, Style, FALSE, 0))
            throw std::runtime_error("Failed to calculate loading screen size.");

        const int Width = Bounds.right - Bounds.left;
        const int Height = Bounds.bottom - Bounds.top;
        const int X = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
        const int Y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
        Window = CreateWindowExW(
            0, ClassName, ApplicationTitle.c_str(), Style, X, Y, Width, Height,
            nullptr, nullptr, Instance, this);
        if (!Window)
            throw std::runtime_error("Failed to create loading screen window.");

        ShowWindow(Window, SW_SHOW);
        UpdateWindow(Window);
        Startup.set_value();
        bStartupReported = true;

        // 창 메시지 또는 종료 이벤트가 도착할 때까지 CPU를 소비하지 않고 기다린다.
        bool bRunning = true;
        while (bRunning)
        {
            const DWORD Result = MsgWaitForMultipleObjectsEx(
                1, &StopEvent, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (Result == WAIT_OBJECT_0) break;
            if (Result != WAIT_OBJECT_0 + 1)
            {
                bCancelRequested.store(true);
                break;
            }

            MSG Message{};
            while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
            {
                if (Message.message == WM_QUIT)
                {
                    bCancelRequested.store(true);
                    bRunning = false;
                    break;
                }
                TranslateMessage(&Message);
                DispatchMessageW(&Message);

                // 메시지가 계속 들어와도 종료 요청을 확인한다.
                if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
                {
                    bRunning = false;
                    break;
                }
            }
        }
    }
    catch (...)
    {
        // 창 준비 전의 실패는 Start로 전달하고, 이후 실패는 취소 상태로 알린다.
        if (!bStartupReported) Startup.set_exception(std::current_exception());
        else bCancelRequested.store(true);
    }

    // 모든 Win32 창 자원은 생성한 UI 스레드에서 해제한다.
    if (Window && IsWindow(Window)) DestroyWindow(Window);
    if (bClassRegistered) UnregisterClassW(ClassName, Instance);
    if (LogoIcon)
    {
        DestroyIcon(LogoIcon);
        LogoIcon = nullptr;
    }
}

// 창에 연결된 관리 객체를 찾아 표시와 취소 요청을 처리한다.
LRESULT CALLBACK FLoadingScreen::WindowProc(
    HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    // CreateWindowExW에 전달한 this를 창의 사용자 데이터에 연결한다.
    auto* Self = reinterpret_cast<FLoadingScreen*>(GetWindowLongPtrW(Window, GWLP_USERDATA));
    if (Message == WM_NCCREATE)
    {
        const auto* CreateInfo = reinterpret_cast<const CREATESTRUCTW*>(LParam);
        Self = static_cast<FLoadingScreen*>(CreateInfo->lpCreateParams);
        SetWindowLongPtrW(Window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Self));
    }
    if (!Self) return DefWindowProcW(Window, Message, WParam, LParam);

    switch (Message)
    {
    case WM_PAINT:
        Self->Paint(Window);
        return 0;
    case WM_ERASEBKGND:
        // Paint에서 배경까지 그리므로 기본 배경 지우기는 생략한다.
        return 1;
    case WM_CLOSE:
        // 진행 중인 로딩을 강제 종료하지 않고 메인 스레드에 취소를 요청한다.
        Self->bCancelRequested.store(true);
        InvalidateRect(Window, nullptr, FALSE);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(Window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(Window, Message, WParam, LParam);
}

// DirectX 없이 Win32 GDI로 로고와 안내 문구를 그린다.
void FLoadingScreen::Paint(HWND Window)
{
    PAINTSTRUCT PaintInfo{};
    HDC DC = BeginPaint(Window, &PaintInfo);
    RECT Client{};
    GetClientRect(Window, &Client);

    // 시스템 브러시와 폰트를 빌려 배경 및 텍스트를 그린다.
    SetDCBrushColor(DC, RGB(25, 28, 32));
    FillRect(DC, &Client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetBkMode(DC, TRANSPARENT);
    SetTextColor(DC, RGB(235, 238, 242));
    HGDIOBJ PreviousFont = SelectObject(DC, GetStockObject(DEFAULT_GUI_FONT));

    constexpr int IconSize = 96;
    const int IconX = (Client.right - IconSize) / 2;
    DrawIconEx(DC, IconX, 32, LogoIcon, IconSize, IconSize, 0, nullptr, DI_NORMAL);

    RECT TitleRect{ 20, 145, Client.right - 20, 175 };
    DrawTextW(DC, ApplicationTitle.c_str(), -1, &TitleRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // 취소 요청 후에도 창을 유지하여 정리를 기다리는 상태를 보여준다.
    const wchar_t* Status = bCancelRequested.load()
        ? L"취소 요청 중입니다. 현재 작업이 끝날 때까지 기다려 주세요."
        : L"프로그램을 준비하고 있습니다...";
    SetTextColor(DC, RGB(165, 175, 188));
    RECT StatusRect{ 20, 190, Client.right - 20, 270 };
    DrawTextW(DC, Status, -1, &StatusRect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

    SelectObject(DC, PreviousFont);
    EndPaint(Window, &PaintInfo);
}