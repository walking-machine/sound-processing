#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "implot.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <imfilebrowser.h>
#include "audio.h"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// Main code
int main(int, char**)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Sound analyzer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    // Setup SDL_Renderer instance
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return false;
    }

    SDL_Texture *img = IMG_LoadTexture(renderer, "the_soul_of_sound-1920x1080.jpg");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);
    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("title");
    fileDialog.SetTypeFilters({ ".wav" });
    audio a;

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin("Audio");
            if (ImGui::Button("Open file")) {
                fileDialog.Open();
            }

            if (a.is_loaded())
            {
                ImGui::Text("Loaded");

                static ImPlotSubplotFlags flags = ImPlotSubplotFlags_LinkAllX;
                if (ImPlot::BeginSubplots("Audio", a.tps.size() + 1, 1, ImVec2(-1,1000), flags)) {
                    
                    if (ImPlot::BeginPlot("Data")) {
                        ImPlot::PlotLine("0", a.get_time_vec().data(), a.get_main_vec().data(), a.num_samples() - 2, 0, 1 * sizeof(double));
                        for (auto &tp : a.tps) {
                            ImPlot::PlotLine(tp.first.c_str(), tp.second.time_vec.data(), tp.second.vals.data(),
                                tp.second.time_vec.size());
                        }
                    ImPlot::EndPlot();
                    }

                    for (auto &tp : a.tps) {
                        if (ImPlot::BeginPlot(tp.first.c_str())) {
                            ImPlot::PlotLine(tp.first.c_str(), tp.second.time_vec.data(), tp.second.vals.data(),
                                tp.second.time_vec.size());
                            ImPlot::EndPlot();
                        }
                    }

                    ImPlot::EndSubplots();
                }

                for (auto &s : a.scalar_vals) {
                    ImGui::Text(s.first.c_str()); ImGui::SameLine();
                    ImGui::Text(std::to_string(s.second).c_str());
                }
            }
            ImGui::End();
        }

        fileDialog.Display();
        if (fileDialog.HasSelected())
        {
            std::cout << "Loading " << fileDialog.GetSelected().string() << "\n";
            a.init(fileDialog.GetSelected().string());
            fileDialog.ClearSelected();
        }

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        SDL_Rect all_screen {0, 0, w, h};
        SDL_RenderCopy(renderer, img, NULL, &all_screen);
        ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::CreateContext();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
