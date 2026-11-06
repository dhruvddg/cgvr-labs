// Offscreen snapshot harness for the native OpenGL labs.
//
// This file does NOT modify CGVR_OpenGLLabs.cpp. It includes it with `main`
// renamed away, so the lab's own draw1()..draw10() and display() are reused
// verbatim, then renders each experiment once, reads the framebuffer back with
// glReadPixels and writes a BMP. Nothing is screen-captured, so no Screen
// Recording permission is involved and the output is pixel-exact.
//
// Build: clang++ -std=c++17 -DGL_SILENCE_DEPRECATION -I../native_opengl \
//          snapshot.cpp -framework OpenGL -framework GLUT -o snapshot
// Run:   ./snapshot <outdir> <1-8|10> [...]

#define main cgvr_lab_main_unused
#include "CGVR_OpenGLLabs.cpp"
#undef main

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

static const int W = 800, H = 600;
static std::vector<int> g_queue;
static std::string g_outDir;
static std::size_t g_index = 0;

// Minimal 24-bit BMP writer. BMP rows run bottom-to-top, which is exactly the
// order glReadPixels returns, so the rows need no flipping.
static bool writeBMP(const std::string &path, const unsigned char *rgb, int w, int h)
{
    const int rowRaw = w * 3;
    const int pad = (4 - (rowRaw % 4)) % 4;
    const int rowSize = rowRaw + pad;
    const uint32_t pixelBytes = static_cast<uint32_t>(rowSize) * h;
    const uint32_t fileSize = 54u + pixelBytes;

    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    unsigned char header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    header[2] = fileSize & 0xFF; header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF; header[5] = (fileSize >> 24) & 0xFF;
    header[10] = 54;                 // pixel data offset
    header[14] = 40;                 // DIB header size
    header[18] = w & 0xFF; header[19] = (w >> 8) & 0xFF;
    header[20] = (w >> 16) & 0xFF; header[21] = (w >> 24) & 0xFF;
    header[22] = h & 0xFF; header[23] = (h >> 8) & 0xFF;
    header[24] = (h >> 16) & 0xFF; header[25] = (h >> 24) & 0xFF;
    header[26] = 1;                  // planes
    header[28] = 24;                 // bits per pixel
    header[34] = pixelBytes & 0xFF; header[35] = (pixelBytes >> 8) & 0xFF;
    header[36] = (pixelBytes >> 16) & 0xFF; header[37] = (pixelBytes >> 24) & 0xFF;
    std::fwrite(header, 1, 54, f);

    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < h; ++y) {
        const unsigned char *src = rgb + static_cast<std::size_t>(y) * rowRaw;
        for (int x = 0; x < w; ++x) {               // RGB -> BGR
            row[x * 3 + 0] = src[x * 3 + 2];
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        std::fwrite(row.data(), 1, rowSize, f);
    }
    std::fclose(f);
    return true;
}

static void captureCurrent()
{
    // Render the experiment through the lab's own display() function.
    experiment = g_queue[g_index];
    glViewport(0, 0, W, H);
    display();

    // display() ends with glutSwapBuffers(), so the finished image is in the
    // back buffer that was just swapped to the front.
    glFinish();
    glReadBuffer(GL_FRONT);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(W) * H * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::string path = g_outDir + "/experiment-" + std::to_string(experiment) + ".bmp";
    if (writeBMP(path, pixels.data(), W, H))
        std::printf("wrote %s\n", path.c_str());
    else
        std::fprintf(stderr, "FAILED to write %s\n", path.c_str());
}

static void tick(int)
{
    if (g_index >= g_queue.size()) { std::exit(0); }
    captureCurrent();
    ++g_index;
    glutTimerFunc(120, tick, 0);
}

static void onDisplay() { /* all drawing is driven from tick() */ }

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: snapshot <outdir> <1-8|10> [more...]\n");
        return 1;
    }
    g_outDir = argv[1];
    for (int i = 2; i < argc; ++i) g_queue.push_back(std::atoi(argv[i]));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(W, H);
    glutCreateWindow("CGVR snapshot");
    glutDisplayFunc(onDisplay);
    glutTimerFunc(250, tick, 0);   // let the context settle before the first read
    glutMainLoop();
    return 0;
}
