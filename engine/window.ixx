module;
#include <SDL3/SDL.h>
#include <iostream>
export module window;

export class Window {
public:
    int Init() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return -1;
        }

        mWindow = SDL_CreateWindow("EM3DLab", 640, 480, SDL_WINDOW_VULKAN);

        if (mWindow == nullptr) {
            auto erros = SDL_GetError();
            std::cerr << "Failed to create window: " << erros << std::endl;
            return -1;
        }
        return 0;
    }
    void Run() {
        SDL_Event e;
        bool bQuit = false;

        while (!bQuit)
        {
            while (SDL_PollEvent(&e) != 0)
            {
                //close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_EVENT_QUIT) bQuit = true;
            }

            // draw();
        }

    }
    ~Window() {
        SDL_DestroyWindow(mWindow);
        SDL_Quit();
    }

    SDL_Window* mWindow;
    // private:
};