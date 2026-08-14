#include <3ds.h>
#include <stdio.h>
#include <string.h>

static inline void putpx(u8 *fb, int x, int y, u8 r, u8 g, u8 b)
{
    if ((unsigned)x >= 400u || (unsigned)y >= 240u) return;

    /* Native 3DS BGR8 framebuffer is rotated: 240 x 400. */
    size_t i = ((size_t)x * 240u + (size_t)(239 - y)) * 3u;
    fb[i + 0] = b;
    fb[i + 1] = g;
    fb[i + 2] = r;
}

static void rect(u8 *fb, int x, int y, int w, int h, u8 r, u8 g, u8 b)
{
    for (int px = x; px < x + w; ++px)
        for (int py = y; py < y + h; ++py)
            putpx(fb, px, py, r, g, b);
}

int main(void)
{
    gfxInitDefault();

    PrintConsole bottom;
    consoleInit(GFX_BOTTOM, &bottom);

    /* Match devkitPro's raw-bitmap example: keep the raw screen single buffered. */
    gfxSetDoubleBuffering(GFX_TOP, false);

    int x = 0;

    while (aptMainLoop())
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;

        u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        memset(fb, 0, 240 * 400 * 3);

        rect(fb, 20, 40, 90, 45, 255, 40, 40);
        rect(fb, 155, 90, 90, 45, 40, 240, 90);
        rect(fb, 290, 140, 90, 45, 30, 190, 255);
        rect(fb, x, 210, 18, 18, 255, 255, 255);

        x = (x + 2) % 382;

        consoleSelect(&bottom);
        printf("\x1b[1;1HRAW FRAMEBUFFER TEST       ");
        printf("\x1b[3;1HWhite block should move.  ");
        printf("\x1b[5;1HSTART exits.              ");

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
