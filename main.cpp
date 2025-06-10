import window;

int main(int argc, char** argv) {
    Window window{};
    if (window.Init() == -1) {
        return -1;
    }
    window.Run();
    return 0;
}
