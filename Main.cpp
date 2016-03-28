#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <SDL.h>
#include <SDL_main.h>

#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace std;

#define myassert(cond) if (!(cond)) { __debugbreak(); }

template <typename T>
T min(T x, T y)
{
    return x < y ? x : y;
}

const float Pi = 3.1415926535897932f;
const float Infinity = 1.0e20f;

float dtor(float d)
{
    return d * Pi / 180.0f;
}

const int ScreenWidth = 1024;
const int ScreenHeight = 768;

struct ScreenPixel
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

ScreenPixel rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return { b, g, r, 255 };
}

void setpix(ScreenPixel* pixels, const int x, const int y, const ScreenPixel& color)
{
    pixels[y * ScreenWidth + x] = color;
}

#if 0

const int MapW = 4;
const int MapH = 4;

// (0,0) at bottom left.
const uint8_t Map[MapW * MapH] =
{
    1, 1, 1, 1,
    1, 0, 0, 1,
    1, 0, 0, 1,
    1, 1, 1, 1
};

#else

const int MapW = 8;
const int MapH = 8;

// (0,0) at bottom left.
const uint8_t Map[MapW * MapH] =
{
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 1,
    1, 0, 0, 1, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1
};

#endif

uint8_t safemap(const int ix, const int iy)
{
    return
        ix >= 0 &&
        iy >= 0 &&
        ix <= MapW - 1 &&
        iy <= MapH - 1
            ? Map[(MapH - 1 - iy) * MapW + ix]
            : 1;
}

uint8_t map(const int ix, const int iy)
{
    myassert(
        ix >= 0 &&
        iy >= 0 &&
        ix <= MapW - 1 &&
        iy <= MapH - 1);
    return safemap(ix, iy);
}

const float HFov = dtor(90.0f);
const float VFov = HFov * ScreenHeight / ScreenWidth;
const float FilmWidth = 0.01f;
const float FilmHeight = FilmWidth * ScreenHeight / ScreenWidth;
const float FocalLength = FilmWidth / 2.0f * tan(HFov / 2.0f);
const float WallHeight = 1.0f;
const float WallPadding = 0.1f;

const float PlayerMoveSpeed = 0.04f;
const float PlayerRotateSpeed = dtor(5.0f);

struct Player
{
    float x, y, a;

    void reset()
    {
        x = 2.0f;
        y = 2.0f;
        a = dtor(90.0f);
    }
};

struct Texture
{
    int w, h, n;
    uint8_t* data;
};

void load_texture(Texture* tex, const char* filepath)
{
    tex->data = stbi_load(filepath, &tex->w, &tex->h, &tex->n, 4);
}

const int NumTextures = 1;
const char* TextureFilePaths[NumTextures] =
{
    "textures/407.png"
};

Texture textures[1];

Player player;
bool minimap = false;

void init()
{
    for (int i = 0; i < NumTextures; ++i)
        load_texture(&textures[i], TextureFilePaths[i]);

    player.reset();
}

void done()
{
    for (int i = 0; i < NumTextures; ++i)
        stbi_image_free(textures[i].data);
}

bool cast_ray(
    const float x0, const float y0,
    const float x1, const float y1,
    float* hx, float* hy,
    float* u)
{
    myassert(x0 >= 0.0f && x0 < static_cast<float>(MapW));
    myassert(y0 >= 0.0f && y0 < static_cast<float>(MapH));

    float x = x0;
    float y = y0;

    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);

    if (x == ix && x1 < x0)
        ix -= 1;

    if (y == iy && y1 < y0)
        iy -= 1;

    myassert(map(ix, iy) == 0);

    const int ix1 = static_cast<int>(x1);
    const int iy1 = static_cast<int>(y1);

    while (true)
    {
        const float dx = x1 - x;
        const float dy = y1 - y;

        float wallx, tx;
        if (dx > 0.0f)
        {
            wallx = ceil(x);
            if (x == wallx)
                wallx += 1.0f;
            myassert(x < wallx);

            tx = (wallx - x) / dx;
            myassert(tx > 0.0f);
        }
        else if (dx < 0.0f)
        {
            wallx = floor(x);
            if (x == wallx)
                wallx -= 1.0f;
            myassert(x > wallx);

            tx = (wallx - x) / dx;
            myassert(tx > 0.0f);
        }
        else tx = Infinity;

        float wally, ty;
        if (dy > 0.0f)
        {
            wally = ceil(y);
            if (y == wally)
                wally += 1.0f;
            myassert(y < wally);

            ty = (wally - y) / dy;
            myassert(ty > 0.0f);
        }
        else if (dy < 0.0f)
        {
            wally = floor(y);
            if (y == wally)
                wally -= 1.0f;
            myassert(y > wally);

            ty = (wally - y) / dy;
            myassert(ty > 0.0f);
        }
        else ty = Infinity;

        myassert(tx < Infinity || ty < Infinity);

        if (tx < ty)
        {
            x = wallx;
            y += tx * dy;

            myassert(dx != 0.0f);
            if (dx > 0.0f)
            {
                ix += 1;
                myassert(ix == static_cast<int>(x));
                myassert(ix <= MapW - 1);
                iy = static_cast<int>(y);
            }
            else
            {
                ix -= 1;
                //myassert(ix == static_cast<int>(x));
                myassert(ix >= 0);
                iy = static_cast<int>(y);
            }

            if (map(ix, iy) != 0)
            {
                *hx = x;
                *hy = y;
                *u = y - iy;
                return true;
            }
        }
        else
        {
            x += ty * dx;
            y = wally;

            myassert(dy != 0.0f);
            if (dy > 0.0f)
            {
                iy += 1;
                myassert(iy == static_cast<int>(y));
                myassert(iy <= MapH - 1);
                ix = static_cast<int>(x);
            }
            else
            {
                iy -= 1;
                //myassert(iy == static_cast<int>(y));
                myassert(iy >= 0);
                ix = static_cast<int>(x);
            }

            if (map(ix, iy) != 0)
            {
                *hx = x;
                *hy = y;
                *u = x - ix;
                return true;
            }
        }

        if (ix == ix1 && iy == iy1)
            return false;
    }
}

void update()
{
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);

    float dx = 0.0f, dy = 0.0f;

    if (keys[SDL_SCANCODE_LEFT])
    {
        if (keys[SDL_SCANCODE_LALT])
        {
            dx += PlayerMoveSpeed * cos(player.a + Pi / 2.0f);
            dy += PlayerMoveSpeed * sin(player.a + Pi / 2.0f);
        }
        else player.a += PlayerRotateSpeed;
    }

    if (keys[SDL_SCANCODE_RIGHT])
    {
        if (keys[SDL_SCANCODE_LALT])
        {
            dx += PlayerMoveSpeed * cos(player.a - Pi / 2.0f);
            dy += PlayerMoveSpeed * sin(player.a - Pi / 2.0f);
        }
        else player.a -= PlayerRotateSpeed;
    }

    if (keys[SDL_SCANCODE_UP])
    {
        dx += PlayerMoveSpeed * cos(player.a);
        dy += PlayerMoveSpeed * sin(player.a);
    }

    if (keys[SDL_SCANCODE_DOWN])
    {
        dx -= PlayerMoveSpeed * cos(player.a);
        dy -= PlayerMoveSpeed * sin(player.a);
    }

    const int ix = static_cast<int>(player.x);
    const int iy = static_cast<int>(player.y);

    myassert(map(ix, iy) == 0);

    for (int y = iy - 1; y <= iy + 1; ++y)
    {
        for (int x = ix - 1; x <= ix + 1; ++x)
        {
            if (x == ix && y == iy)
                continue;

            if (safemap(x, y) != 0)
            {
                const float blockx0 = x - WallPadding;
                const float blocky0 = y - WallPadding;
                const float blockx1 = x + 1 + WallPadding;
                const float blocky1 = y + 1 + WallPadding;

                if (dx > 0.0f && safemap(x - 1, y) == 0)
                {
                    const float t = (blockx0 - player.x) / dx;
                    if (t >= 0.0f && t < 1.0f)
                    {
                        const float ay = player.y + t * dy;
                        if (ay >= blocky0 && ay <= blocky1)
                        {
                            dx = blockx0 - player.x;
                        }
                    }
                }

                if (dx < 0.0f && safemap(x + 1, y) == 0)
                {
                    const float t = (blockx1 - player.x) / dx;
                    if (t >= 0.0f && t < 1.0f)
                    {
                        const float ay = player.y + t * dy;
                        if (ay >= blocky0 && ay <= blocky1)
                        {
                            dx = blockx1 - player.x;
                        }
                    }
                }

                if (dy > 0.0f && safemap(x, y - 1) == 0)
                {
                    const float t = (blocky0 - player.y) / dy;
                    if (t >= 0.0f && t < 1.0f)
                    {
                        const float ax = player.x + t * dx;
                        if (ax >= blockx0 && ax <= blockx1)
                        {
                            dy = blocky0 - player.y;
                        }
                    }
                }

                if (dy < 0.0f && safemap(x, y + 1) == 0)
                {
                    const float t = (blocky1 - player.y) / dy;
                    if (t >= 0.0f && t < 1.0f)
                    {
                        const float ax = player.x + t * dx;
                        if (ax >= blockx0 && ax <= blockx1)
                        {
                            dy = blocky1 - player.y;
                        }
                    }
                }
            }
        }
    }

    player.x += dx;
    player.y += dy;
}

void renderview(ScreenPixel* pixels)
{
    for (int x = 0; x < ScreenWidth; ++x)
    {
        for (int y = 0; y < ScreenHeight; ++y)
            pixels[y * ScreenWidth + x] = rgb(150, 150, 150);

        const float sx = (FilmWidth * 0.5f) - (x + 0.5f) * (FilmWidth / ScreenWidth);
        const float a = player.a + atan2(sx, FocalLength);

        const float MaxDist = 1000.0f;
        float hx, hy;
        float u;
        if (cast_ray(
                player.x, player.y,
                player.x + MaxDist * cos(a),
                player.y + MaxDist * sin(a),
                &hx, &hy,
                &u))
        {
            const float dx = hx - player.x;
            const float dy = hy - player.y;
            const float d = sqrt(dx * dx + dy * dy) * cos(a - player.a);
            const float h = FocalLength * WallHeight / d;

            const int wallheight = static_cast<int>(h / FilmHeight * ScreenHeight);

            const float shade = 1.0f - min(d / 8.0f, 1.0f);

            const int iu = static_cast<int>(u * textures[0].w);

            for (int y = 0; y < wallheight; ++y)
            {
                const int py = ScreenHeight / 2 + y - wallheight / 2;
                if (py < 0 || py > ScreenHeight - 1)
                    continue;

                const float v = static_cast<float>(y + 0.5) / wallheight;
                const int iv = static_cast<int>(v * textures[0].h);
                const uint8_t* tex = &textures[0].data[(iv * textures[0].w + iu) * 4];
                pixels[py * ScreenWidth + x] =
                    rgb(
                        static_cast<uint8_t>(tex[0] * shade),
                        static_cast<uint8_t>(tex[1] * shade),
                        static_cast<uint8_t>(tex[2] * shade));
            }
        }
    }
}

void rendermap(ScreenPixel* pixels)
{
    const int CellSize = 40;

    for (int y = 0; y < MapH * CellSize; ++y)
    {
        for (int x = 0; x < MapW * CellSize; ++x)
        {
            ScreenPixel color;

            const float wx = static_cast<float>(x) / CellSize;
            const float wy = static_cast<float>(y) / CellSize;

            const int ix = static_cast<int>(wx);
            const int iy = static_cast<int>(wy);

            const uint8_t cell = map(ix, iy);
            color = cell == 0 ? rgb(150, 150, 150) : rgb(220, 220, 220);

            const float fx = wx - ix;
            const float fy = wy - iy;

            if (cell == 0)
            {
                if ((fx <= WallPadding && safemap(ix - 1, iy) != 0) ||
                    (fx >= 1.0f - WallPadding && safemap(ix + 1, iy) != 0) ||
                    (fy <= WallPadding && safemap(ix, iy - 1) != 0) ||
                    (fy >= 1.0f - WallPadding && safemap(ix, iy + 1) != 0) ||
                    (fx <= WallPadding && fy <= WallPadding && safemap(ix - 1, iy - 1) != 0) ||
                    (fx >= 1.0f - WallPadding && fy <= WallPadding && safemap(ix + 1, iy - 1) != 0) ||
                    (fx <= WallPadding && fy >= 1.0f - WallPadding && safemap(ix - 1, iy + 1)) ||
                    (fx >= 1.0f - WallPadding && fy >= 1.0f - WallPadding && safemap(ix + 1, iy + 1) != 0))
                    color = rgb(150, 180, 150);
            }

            const float dpx = wx - player.x;
            const float dpy = wy - player.y;
            if (sqrt(dpx * dpx + dpy * dpy) <= 0.05f)
                color = rgb(255, 0, 0);

            setpix(pixels, x, MapH * CellSize - 1 - y, color);
        }
    }
}

void render(ScreenPixel* pixels)
{
    renderview(pixels);

    if (minimap)
        rendermap(pixels);
}

extern "C" int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow(
            "Wolfie",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(ScreenWidth),
            static_cast<int>(ScreenHeight),
            SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Texture* screen_texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(ScreenWidth),
            static_cast<int>(ScreenHeight));

    ScreenPixel* pixels = new ScreenPixel[ScreenWidth * ScreenHeight];
    
    init();

    bool quit = false;
    while (!quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
              case SDL_QUIT:
                quit = true;
                break;

              case SDL_KEYDOWN:
                switch (e.key.keysym.sym)
                {
                  case SDLK_ESCAPE:
                    quit = true;
                    break;

                  case SDLK_r:
                    player.reset();
                    break;

                  case SDLK_TAB:
                    minimap = !minimap;
                    break;
                }

              default:
                break;
            }
        }

        update();
        render(pixels);

        SDL_UpdateTexture(screen_texture, nullptr, pixels, ScreenWidth * sizeof(ScreenPixel));

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    done();
    SDL_Quit();

    return 0;
}
