#include "EngineLoop.h"

#include "FWindowsPlatformTime.h"
#include "ImGuiManager.h"
#include "World.h"
#include "Camera/CameraComponent.h"
#include "PropertyEditor/ViewportTypePanel.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/UnrealEd.h"
#include "UnrealClient.h"
#include "slate/Widgets/Layout/SSplitter.h"
#include "LevelEditor/SLevelEditor.h"
#include "UnrealEd\SceneMgr.h"
#include "OctreeNode.h"


extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }
    int zDelta = 0;
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            //UGraphicsDevice 객체의 OnResize 함수 호출
            if (FEngineLoop::GraphicDevice.SwapChain)
            {
                FEngineLoop::Renderer.PrepareResize();
                FEngineLoop::GraphicDevice.OnResize(hWnd);
                FEngineLoop::Renderer.OnResize(FEngineLoop::GraphicDevice.SwapchainDesc);
            }
            
            if (GEngineLoop.GetLevelEditor())
            {
                if (FEditorViewportClient* ViewportClient = GEngineLoop.GetLevelEditor()->GetViewports()[0].get())
                {
                    ViewportClient->ResizeViewport(FEngineLoop::GraphicDevice.SwapchainDesc);
                    FEngineLoop::GraphicDevice.DeviceContext->RSSetViewports(1, &ViewportClient->GetD3DViewport());
                }
            }
        }
        Console::GetInstance().OnResize(hWnd);
        // ControlPanel::GetInstance().OnResize(hWnd);
        // PropertyPanel::GetInstance().OnResize(hWnd);
        // Outliner::GetInstance().OnResize(hWnd);
        // ViewModeDropdown::GetInstance().OnResize(hWnd);
        // ShowFlags::GetInstance().OnResize(hWnd);
        if (GEngineLoop.GetUnrealEditor())
        {
            GEngineLoop.GetUnrealEditor()->OnResize(hWnd);
        }
        ViewportTypePanel::GetInstance().OnResize(hWnd);
        break;
    case WM_MOUSEWHEEL:
        zDelta = GET_WHEEL_DELTA_WPARAM(wParam); // 휠 회전 값 (+120 / -120)
        if (GEngineLoop.GetLevelEditor())
        {
            if (GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->IsPerspective())
            {
                if (GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetIsOnRBMouseClick())
                {
                    GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->SetCameraSpeedScalar(
                        static_cast<float>(GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetCameraSpeedScalar() + zDelta * 0.01)
                    );
                }
                else
                {
                    GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->CameraMoveForward(zDelta * 0.1f);
                }
            }
            else
            {
                FEditorViewportClient::SetOthoSize(-zDelta * 0.01f);
            }
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

FGraphicsDevice FEngineLoop::GraphicDevice;
FRenderer FEngineLoop::Renderer;
FResourceMgr FEngineLoop::ResourceManager;
uint32 FEngineLoop::TotalAllocationBytes = 0;
uint32 FEngineLoop::TotalAllocationCount = 0;

FEngineLoop::FEngineLoop()
    : hWnd(nullptr)
    , GWorld(nullptr)
    , LevelEditor(nullptr)
    , UnrealEditor(nullptr)
{
}

int32 FEngineLoop::PreInit()
{
    return 0;
}

int32 FEngineLoop::Init(HINSTANCE hInstance)
{
    WindowInit(hInstance);
    
    GraphicDevice.Initialize(hWnd);
    Renderer.Initialize(&GraphicDevice);

    UIMgr = new UImGuiManager;
    UIMgr->Initialize(hWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);

    ResourceManager.Initialize(&Renderer, &GraphicDevice);
    LevelEditor = new SLevelEditor();
    LevelEditor->Initialize();
    Renderer.SetViewport(LevelEditor->GetActiveViewportClient());

    GWorld = new UWorld;
#ifdef _DEBUG
    //FString JsonStr = FSceneMgr::LoadSceneFromFile("Default1.scene");
#else
    FString JsonStr = FSceneMgr::LoadSceneFromFile("Default.scene");
    SceneData Scene = FSceneMgr::ParseSceneData(JsonStr);
    GWorld->LoadSceneData(Scene, LevelEditor->GetActiveViewportClient());
#endif
    GWorld->Initialize(hWnd);
    Renderer.SetWorld(GWorld);

    LevelEditor->OffMultiViewport();
    
    Renderer.PrepareRender(true); // Force update
    
    return 0;
}


void FEngineLoop::Render(bool bShouldUpdateRender)
{
    Renderer.PrepareRender(bShouldUpdateRender);
    Renderer.Render();
}

void FEngineLoop::QuadRender()
{
    Renderer.PrepareQuad();
    Renderer.RenderQuad();
}

void FEngineLoop::Tick()
{
    double TargetDeltaTime = -1.f;
    
    bool bShouldLimitFPS = TargetFPS > 0;
    if (bShouldLimitFPS)
    {
        // Limit FPS
        TargetDeltaTime = 1000.0f / static_cast<double>(TargetFPS); // 1 FPS's target time (ms)
    }

    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    LARGE_INTEGER StartTime;
    QueryPerformanceCounter(&StartTime);
    
    float ElapsedTime = 1.0;

    while (bIsExit == false)
    {
        const LARGE_INTEGER EndTime = StartTime;
        QueryPerformanceCounter(&StartTime);

        ElapsedTime = static_cast<float>(StartTime.QuadPart - EndTime.QuadPart) / static_cast<float>(Frequency.QuadPart);

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); // 키보드 입력 메시지를 문자메시지로 변경
            DispatchMessage(&msg);  // 메시지를 WndProc에 전달

            if (msg.message == WM_QUIT)
            {
                bIsExit = true;
                break;
            }
        }

        bool bShouldUpdateRender = false;
        bShouldUpdateRender |= GWorld->Tick(ElapsedTime);
        bShouldUpdateRender |= LevelEditor->Tick(ElapsedTime);
        if (bShouldUpdateRender || true) // 억제기
        {
            Render(bShouldUpdateRender);
        }

        // Final Render
        QuadRender();

        UIMgr->BeginFrame();

        Console::GetInstance().Draw();

        UIMgr->EndFrame();

        GraphicDevice.SwapBuffer();

        if (bShouldLimitFPS)
        {
            LimitFPS(StartTime, Frequency, TargetDeltaTime);
        }
    }
}

void FEngineLoop::LimitFPS(const LARGE_INTEGER& StartTime, const LARGE_INTEGER& Frequency, double TargetDeltaTime) const
{
    double ElapsedTime;
    do
    {
        Sleep(0);

        LARGE_INTEGER CurrentTime;
        QueryPerformanceCounter(&CurrentTime);

        ElapsedTime = static_cast<double>(CurrentTime.QuadPart - StartTime.QuadPart) * 1000.0 / static_cast<double>(Frequency.QuadPart);
    } while (ElapsedTime < TargetDeltaTime);
}

float FEngineLoop::GetAspectRatio(IDXGISwapChain* swapChain) const
{
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    return static_cast<float>(desc.BufferDesc.Width) / static_cast<float>(desc.BufferDesc.Height);
}

void FEngineLoop::Input()
{
    // W04
    /*if (GetAsyncKeyState('M') & 0x8000)
    {
        if (!bTestInput)
        {
            bTestInput = true;
            if (LevelEditor->IsMultiViewport())
            {
                LevelEditor->OffMultiViewport();
            }
            else
                LevelEditor->OnMultiViewport();
        }
    }
    else
    {
        bTestInput = false;
    }*/
}

void FEngineLoop::Exit()
{
    LevelEditor->Release();
    GWorld->Release();
    delete GWorld;
    UIMgr->Shutdown();
    delete UIMgr;
    ResourceManager.Release(&Renderer);
    Renderer.Release();
    GraphicDevice.Release();
}


void FEngineLoop::WindowInit(HINSTANCE hInstance)
{
    WCHAR WindowClass[] = L"JungleWindowClass";

    WCHAR Title[] = L"Game Tech Lab";

    WNDCLASSW wndclass = {0};
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.lpszClassName = WindowClass;

    RegisterClassW(&wndclass);

    hWnd = CreateWindowExW(
        0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 1000,
        nullptr, nullptr, hInstance, nullptr
    );
}
