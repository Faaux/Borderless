#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <shellapi.h>
#include <strsafe.h>
#pragma comment( \
    linker,      \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

#include "resource.h"
#undef near
#undef far

#include <gl/glew.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

// Tray Stuff
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

static void handle_error(LPCTSTR lpszFunction)
{
    auto dw = GetLastError();
    if (FAILED(dw))
    {
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
                                          (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen(lpszFunction) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                        TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);
        MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

        LocalFree(lpMsgBuf);
        LocalFree(lpDisplayBuf);
    }
}

static bool add_notification_icon(HWND hwnd)
{
    auto instance = GetModuleHandle(NULL);

    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_NOTIFICATIONICON), LIM_SMALL, &nid.hIcon);
    handle_error("LoadIconMetric");

    LoadString(instance, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    handle_error("LoadString");

    Shell_NotifyIcon(NIM_ADD, &nid);
    handle_error("Shell_NotifyIcon NIM_ADD");

    // NOTIFYICON_VERSION_4 is preferred
    nid.uVersion = NOTIFYICON_VERSION_4;
    auto result = Shell_NotifyIcon(NIM_SETVERSION, &nid);
    handle_error("Shell_NotifyIcon NIM_SETVERSION");
    return result;
}

static bool delete_notification_icon(HWND hwnd)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

// GLFW Stuff

static void glfw_error_callback(int error, const char *description)
{
    // fmt::print(stderr, "Glfw Error {}: {}\n", error, description);
}

static GLFWwindow *main_window;
static HWND main_window_hwnd;
static bool should_close = false;
static bool is_popup = false;
static int last_pos_x = 0;
static int last_pos_y = 0;
static int last_size_x = 0;
static int last_size_y = 0;

struct Window
{
    HWND hwnd;
    std::string name;
    bool active = false;
    LONG window_style;
    int pos_x = 1280;
    int pos_y = 0;
    int size_x = 2560;
    int size_y = 1440;
};

static std::vector<Window> windows;

static std::pair<int, int> find_position(int pos_x, int pos_y, int size_x, int size_y)
{
    int count;
    GLFWmonitor **monitors = glfwGetMonitors(&count);

    for (int i = 0; i < count; i++)
    {
        GLFWmonitor *monitor = monitors[i];
        int x, y;
        glfwGetMonitorPos(monitor, &x, &y);

        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        auto width = mode->width + x;
        auto height = mode->height + y;

        // Check if in rect
        if (pos_x > x && pos_x < width && pos_y > y && pos_y < +height)
        {
            if (pos_x + size_x > width)
            {
                pos_x -= size_x;
            }
            if (pos_y + size_y > height)
            {
                pos_y -= size_y;
            }
            return {pos_x, pos_y};
        }
    }
    return {pos_x, pos_y};
}

static void activate_popup(int pos_x, int pos_y)
{
    if (!is_popup)
    {
        glfwGetWindowSize(main_window, &last_size_x, &last_size_y);
        glfwGetWindowPos(main_window, &last_pos_x, &last_pos_y);
    }

    glfwSetWindowAttrib(main_window, GLFW_RESIZABLE, false);
    glfwSetWindowAttrib(main_window, GLFW_DECORATED, false);
    glfwSetWindowAttrib(main_window, GLFW_FLOATING, true);
    glfwSetWindowAttrib(main_window, GLFW_FOCUS_ON_SHOW, true);
    is_popup = true;

    glfwSetWindowSize(main_window, 200, 400);
    auto [x, y] = find_position(pos_x, pos_y, 200, 400);
    glfwSetWindowPos(main_window, x, y);

    glfwShowWindow(main_window);
}

static void activate_main()
{
    glfwSetWindowSize(main_window, last_size_x, last_size_y);
    glfwSetWindowPos(main_window, last_pos_x, last_pos_y);

    glfwSetWindowAttrib(main_window, GLFW_RESIZABLE, true);
    glfwSetWindowAttrib(main_window, GLFW_DECORATED, true);
    glfwSetWindowAttrib(main_window, GLFW_FLOATING, false);
    glfwSetWindowAttrib(main_window, GLFW_FOCUS_ON_SHOW, true);
    is_popup = false;
    glfwShowWindow(main_window);
    delete_notification_icon(main_window_hwnd);
}

void window_focus_callback(GLFWwindow *window, int focused)
{
    if (!focused && is_popup)
    {
        glfwHideWindow(main_window);
    }
}

static LRESULT window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC default_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (message)
    {
        case WMAPP_NOTIFYCALLBACK:
        {
            switch (LOWORD(lParam))
            {
                case NIN_SELECT:
                {
                    activate_main();
                    break;
                }
                case WM_CONTEXTMENU:
                {
                    activate_popup(LOWORD(wParam), HIWORD(wParam));
                    break;
                }
            }
            break;
        }

        case WM_DESTROY:
        {
            delete_notification_icon(hwnd);
            return CallWindowProc(default_proc, hwnd, message, wParam, lParam);
        }
    }
    return CallWindowProc(default_proc, hwnd, message, wParam, lParam);
}

static void patch_window_proc(HWND hwnd)
{
    static_assert(sizeof(WNDPROC) == sizeof(LONG_PTR));
    auto proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)proc);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)window_proc);
}

static void update_windows()
{
    std::erase_if(windows, [](const auto &proc) { return !IsWindow(proc.hwnd); });

    for (auto &proc : windows)
    {
        char buf[2048];
        auto len = GetWindowText(proc.hwnd, buf, sizeof(buf));
        proc.name = std::string(buf, len);
    }
}

static void render_main_window()
{
    auto reset_ui = false;

    // Main Docking Node
    {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our backgroundand handle
        // the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("Close"))
                {
                    should_close = true;
                }

                if (ImGui::MenuItem("Reset UI"))
                {
                    reset_ui = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (reset_ui)
        {
            reset_ui = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);  // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGui::DockBuilderDockWindow("Main Menu", dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();
    }

    if (ImGui::Begin("Main Menu"))
    {
        for (auto &proc : windows)
        {
            if (ImGui::CollapsingHeader(proc.name.c_str()))
            {
                if (ImGui::Checkbox("Active", &proc.active))
                {
                    if (proc.active)
                    {
                        // Save style
                        proc.window_style = GetWindowLong(proc.hwnd, GWL_STYLE);
                    }
                    else
                    {
                        // Restore
                        SetWindowLong(proc.hwnd, GWL_STYLE, proc.window_style);
                    }
                }
                ImGui::DragInt2("Position", &proc.pos_x, 1, 0, 16000, "%d", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragInt2("Size", &proc.size_x, 1, 0, 16000, "%d", ImGuiSliderFlags_AlwaysClamp);
            }

            if (proc.active)
            {
                auto style =
                    proc.window_style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
                SetWindowLong(proc.hwnd, GWL_STYLE, style);
                MoveWindow(proc.hwnd, proc.pos_x, proc.pos_y, proc.size_x, proc.size_y, true);
            }
        }
    }
    ImGui::End();
}

static void render_popup() {}

static void win_event_proc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                           DWORD idEventThread, DWORD dwmsEventTime)
{
    if (hwnd && idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
    {
        glfwPostEmptyEvent();
        for (auto it = windows.begin(); it < windows.end(); it++)
        {
            const auto &proc = *it;
            if (proc.hwnd == hwnd)
            {
                if (event == EVENT_OBJECT_DESTROY)
                {
                    windows.erase(it);
                }
                return;
            }
        }

        if (event == EVENT_OBJECT_DESTROY)
        {
            return;
        }

        auto &proc = windows.emplace_back();
        proc.hwnd = hwnd;

        char buf[256];
        auto len = GetWindowText(hwnd, buf, sizeof(buf));
        if (len == 0)
        {
            windows.pop_back();
        }
        else
        {
            proc.name = std::string(buf, len);
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    if (!glfwInit())
    {
        std::terminate();
    }

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_VISIBLE, true);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    main_window = glfwCreateWindow(1280, 720, "Borderless", NULL, NULL);
    if (!main_window)
    {
        std::terminate();
    }
    glfwMakeContextCurrent(main_window);
    glfwSwapInterval(1);  // Enable vsync
    main_window_hwnd = glfwGetWin32Window(main_window);
    patch_window_proc(main_window_hwnd);

    glfwSetWindowFocusCallback(main_window, window_focus_callback);

    auto foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, win_event_proc, 0,
                                           0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    auto delete_hook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, win_event_proc, 0, 0,
                                       WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Start a timer thread
    std::thread(
        []()
        {
            using namespace std::chrono_literals;
            while (true)
            {
                std::this_thread::sleep_for(500ms);
                glfwPostEmptyEvent();
            }
        })
        .detach();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(main_window, true);
    ImGui_ImplOpenGL3_Init();

    while (!should_close)
    {
        if (glfwWindowShouldClose(main_window))
        {
            if (!is_popup)
            {
                glfwGetWindowSize(main_window, &last_size_x, &last_size_y);
                glfwGetWindowPos(main_window, &last_pos_x, &last_pos_y);
            }
            glfwSetWindowShouldClose(main_window, false);
            glfwHideWindow(main_window);
            add_notification_icon(main_window_hwnd);
        }

        glfwWaitEvents();
        update_windows();

        if (!glfwGetWindowAttrib(main_window, GLFW_VISIBLE))
        {
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (is_popup)
        {
            render_popup();
        }
        else
        {
            render_main_window();
        }

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(main_window, &w, &h);

        glViewport(0, 0, w, h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(main_window);
    }

    UnhookWinEvent(foreground_hook);
    UnhookWinEvent(delete_hook);

    delete_notification_icon(main_window_hwnd);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(main_window);

    glfwTerminate();

    return 0;
}
