#include "Console.h"
#include <cstdarg>
#include <cstdio>

#include "UnrealEd/EditorViewportClient.h"

#include "World.h"
#include "Actors/Player.h"


extern float CPUElapsedTime;
extern float GPUElapsedTime;

// 싱글톤 인스턴스 반환
Console& Console::GetInstance() {
    static Console instance;
    return instance;
}

void StatOverlay::Render(ID3D11DeviceContext* context, UINT width, UINT height)
 {

        if (!showRender)
            return;
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        // 창 크기를 화면의 50%로 설정합니다.
        ImVec2 windowSize(displaySize.x * 0.7f, displaySize.y * 0.1f);
        // 창을 중앙에 배치하기 위해 위치를 계산합니다.
        // ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);
        ImVec2 windowPos(0, 0);

    
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Begin("Stat Overlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar);
        if (showFPS) {
            static float lastTime = ImGui::GetTime();
            static int frameCount = 0;
            static float fps = 0.0f;

            frameCount++;
            float currentTime = ImGui::GetTime();
            float deltaTime = currentTime - lastTime;

            if (deltaTime >= 1.0f) { // 1초마다 FPS 업데이트
                fps = frameCount / deltaTime;
                frameCount = 0;
                lastTime = currentTime;
            }

            static float lastTimeEveryFrame = ImGui::GetTime();
            float deltaTimeEveryFrame = currentTime - lastTimeEveryFrame;
            lastTimeEveryFrame = currentTime;

            ImGui::Text("FPS: %.2f / deltaTime : %lf", fps, deltaTimeEveryFrame * 1000);
        }

        AEditorPlayer* player = GEngineLoop.GetWorld()->GetEditorPlayer();
        double curPickingTime = player->GetCurPickingTime();
        uint32 totalPickingCount = player->GetTotalPickCount();
        double accumulatedPickingTime = player->GetAccumulatedPickingTime();
        ImGui::Text("Picking Time %lf ms: Num Attempts %u : Accumulated Time %lf ms", curPickingTime, totalPickingCount, accumulatedPickingTime);
        ImGui::Text("CPU Time %lf / GPU Time : %lf", CPUElapsedTime* 1000, GPUElapsedTime* 1000);

        if (showMemory)
        {
            ImGui::Text("Allocated Object Count: %llu", FPlatformMemory::GetAllocationCount<EAT_Object>());
            ImGui::Text("Allocated Object Memory: %llu B", FPlatformMemory::GetAllocationBytes<EAT_Object>());
            ImGui::Text("Allocated Container Count: %llu", FPlatformMemory::GetAllocationCount<EAT_Container>());
            ImGui::Text("Allocated Container memory: %llu B", FPlatformMemory::GetAllocationBytes<EAT_Container>());
        }
        ImGui::PopStyleColor();
        ImGui::End();
    }

// 생성자
Console::Console() {}

// 소멸자
Console::~Console() {}

// 로그 초기화
void Console::Clear() {
    items.Empty();
}

// 로그 추가
void Console::AddLog(LogLevel level, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    items.Add({ level, std::string(buf) });
    scrollToBottom = true;
}


// 콘솔 창 렌더링
void Console::Draw() {
    if (!bWasOpen) return;
    // 창 크기 및 위치 계산
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    
    // 콘솔 창의 크기와 위치 설정
    float expandedHeight = displaySize.y * 0.4f; // 확장된 상태일 때 높이 (예: 화면의 40%)
    float collapsedHeight = 30.0f;               // 축소된 상태일 때 높이
    float currentHeight = bExpand ? expandedHeight : collapsedHeight;
    
    // 왼쪽 하단에 위치하도록 계산 (창의 좌측 하단이 화면의 좌측 하단에 위치)
    ImVec2 windowSize(displaySize.x * 0.5f, currentHeight); // 폭은 화면의 40%
    ImVec2 windowPos(0, displaySize.y - currentHeight);
    
    // 창 위치와 크기를 고정
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    // 창을 표시하고 닫힘 여부 확인
    overlay.Render(GEngineLoop.GraphicDevice.DeviceContext, width, height);
    // bExpand = ImGui::Begin("Console", &bWasOpen);
    // if (!bExpand) {
    //     ImGui::End();
    //     return;
    // }
    //
    // // 창을 접었을 경우 UI를 표시하지 않음
    // if (!bExpand) {
    //     ImGui::End();
    //     return;
    // }
    //
    // // 버튼 UI (로그 수준별 추가)
    // if (ImGui::Button("Clear")) { Clear(); }
    // ImGui::SameLine();
    // if (ImGui::Button("Copy")) { ImGui::LogToClipboard(); }
    //
    // ImGui::Separator();
    //
    // // 필터 입력 창
    // ImGui::Text("Filter:");
    // ImGui::SameLine();
    //
    // filter.Draw("##Filter", 100);
    //
    // ImGui::SameLine();
    //
    // // 로그 수준을 선택할 체크박스
    // ImGui::Checkbox("Show Display", &showLogTemp);
    // ImGui::SameLine();
    // ImGui::Checkbox("Show Warning", &showWarning);
    // ImGui::SameLine();
    // ImGui::Checkbox("Show Error", &showError);
    //
    // ImGui::Separator();
    // // 로그 출력 (필터 적용)
    // ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    // for (const auto& entry : items) {
    //     if (!filter.PassFilter(*entry.message)) continue;
    //
    //     // 로그 수준에 맞는 필터링
    //     if ((entry.level == LogLevel::Display && !showLogTemp) ||
    //         (entry.level == LogLevel::Warning && !showWarning) ||
    //         (entry.level == LogLevel::Error && !showError)) {
    //         continue;
    //     }
    //
    //     // 색상 지정
    //     ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    //     switch (entry.level) {
    //     case LogLevel::Display: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;  // 기본 흰색
    //     case LogLevel::Warning: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // 노란색
    //     case LogLevel::Error:   color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break; // 빨간색
    //     }
    //
    //     ImGui::TextColored(color, "%s", *entry.message);
    // }
    // if (scrollToBottom) {
    //     ImGui::SetScrollHereY(1.0f);
    //     scrollToBottom = false;
    // }
    // ImGui::EndChild();
    //
    // ImGui::Separator();
    //
    // // 입력창
    // bool reclaimFocus = false;
    // if (ImGui::InputText("Input", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
    //     if (inputBuf[0]) {
    //         AddLog(LogLevel::Display, ">> %s", inputBuf);
    //         std::string command(inputBuf);
    //         ExecuteCommand(command);
    //         history.Add(std::string(inputBuf));
    //         historyPos = -1;
    //         scrollToBottom = true; // 자동 스크롤
    //     }
    //     inputBuf[0] = '\0';
    //     reclaimFocus = true;
    // }
    //
    // // 입력 필드에 자동 포커스
    // if (reclaimFocus) {
    //     ImGui::SetKeyboardFocusHere(-1);
    // }
    //
    // ImGui::End();
}

void Console::ExecuteCommand(const std::string& command)
{
    AddLog(LogLevel::Display, "Executing command: %s", command.c_str());

    if (command == "clear")
    {
        Clear();
    }
    else if (command == "help")
    {
        AddLog(LogLevel::Display, "Available commands:");
        AddLog(LogLevel::Display, " - clear: Clears the console");
        AddLog(LogLevel::Display, " - help: Shows available commands");
        AddLog(LogLevel::Display, " - stat fps: Toggle FPS display");
        AddLog(LogLevel::Display, " - stat memory: Toggle Memory display");
        AddLog(LogLevel::Display, " - stat none: Hide all stat overlays");
    }
    else if (command.rfind("stat ", 0) == 0) { // stat 명령어 처리
        overlay.ToggleStat(command);
    }
    else {
        AddLog(LogLevel::Error, "Unknown command: %s", command.c_str());
    }
}

void Console::OnResize(HWND hWindow)
{
    RECT clientRect;
    GetClientRect(hWindow, &clientRect);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
}

