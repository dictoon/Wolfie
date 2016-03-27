#include <SDL.h>
#include <SDL_main.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace std;

const size_t ScreenWidth = 800;
const size_t ScreenHeight = 600;

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

ScreenPixel rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return { b, g, r, a };
}

const size_t MapW = 8;
const size_t MapH = 8;

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

uint8_t map(const int ix, const int iy)
{
    assert(
        ix >= 0 &&
        iy >= 0 &&
        ix <= static_cast<int>(MapW) - 1 &&
        iy <= static_cast<int>(MapH) - 1);
    return
        ix >= 0 &&
        iy >= 0 &&
        ix <= static_cast<int>(MapW) - 1 &&
        iy <= static_cast<int>(MapH) - 1
            ? Map[(MapH - iy - 1) * MapW + ix]
            : 1;
}

const float Pi = 3.1415926535897932f;
const float HFov = 90.0f * Pi / 180.0f;
const float VFov = HFov * ScreenHeight / ScreenWidth;
const float PlayerMoveSpeed = 0.03f;
const float PlayerRotateSpeed = 3.0f;
const float FilmWidth = 0.01f;
const float FilmHeight = FilmWidth * ScreenHeight / ScreenWidth;
const float FocalLength = FilmWidth / 2.0f * tan(HFov / 2.0f);
const float WallHeight = 1.0f;
const float Infinity = 1.0e20f;

struct Player
{
    float x, y, a;
};

Player player;

void init()
{
    player.x = 2.1f;
    player.y = 2.1f;
    player.a = 0.0f;
}

bool cast_ray(
    const float x0, const float y0,
    const float x1, const float y1,
    float* hx, float* hy)
{
    int ix = static_cast<int>(floor(x0));
    int iy = static_cast<int>(floor(y0));
    float x = x0;
    float y = y0;

    while (true)
    {
        const float dx = x1 - x;
        const float dy = y1 - y;

        // Intersect vertical walls.
        float tx = Infinity;
        if (dx != 0.0f)
        {
            const float wallx = dx > 0.0f ? ix + 1.0f : ix;
            const float t = (wallx - x) / dx;
            assert(t >= 0.0f);
            if (t > 0.0f && t <= 1.0f)
                tx = t;
        }

        // Intersect horizontal walls.
        float ty = Infinity;
        if (dy != 0.0f)
        {
            const float wally = dy > 0.0f ? iy + 1.0f : iy;
            const float t = (wally - y) / dy;
            assert(t >= 0.0f);
            if (t > 0.0f && t <= 1.0f)
                ty = t;
        }

        if (tx < ty)
        {
            // Hit vertical wall first.
            x += tx * dx;
            y += tx * dy;
            ix += dx > 0.0f ? 1 : -1;
            if (map(ix, iy))
            {
                *hx = x;
                *hy = y;
                return true;
            }
        }
        else if (ty < tx)
        {
            // Hit horizontal wall first.
            x += ty * dx;
            y += ty * dy;
            iy += dy > 0.0f ? 1 : -1;
            if (map(ix, iy))
            {
                *hx = x;
                *hy = y;
                return true;
            }
        }
        else
        {
            // No hit.
            return false;
        }
    }
}

void update()
{
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);

    const float rotate =
        keys[SDL_SCANCODE_LEFT] ? +PlayerRotateSpeed :
        keys[SDL_SCANCODE_RIGHT] ? -PlayerRotateSpeed : 0.0f;

    const float move =
        keys[SDL_SCANCODE_UP] ? +PlayerMoveSpeed :
        keys[SDL_SCANCODE_DOWN] ? -PlayerMoveSpeed : 0.0f;

    player.a += rotate * Pi / 180.0f;

    if (move != 0.0f)
    {
        const float dx = move * cos(player.a);
        const float dy = move * sin(player.a);

        player.x += dx;
        player.y += dy;

        const float Margin = 0.2f;
        float hx, hy;

        const float sensex = player.x + (dx > 0.0f ? Margin : -Margin);
        if (cast_ray(
                player.x, player.y,
                sensex, player.y,
                &hx, &hy))
        {
            player.x -= sensex - hx;
        }

        const float sensey = player.y + (dy > 0.0f ? Margin : -Margin);
        if (cast_ray(
                player.x, player.y,
                player.x, sensey,
                &hx, &hy))
        {
            player.y -= sensey - hy;
        }
    }
}

void render(ScreenPixel* pixels)
{
    for (size_t x = 0; x < ScreenWidth; ++x)
    {
        for (size_t y = 0; y < ScreenHeight; ++y)
            pixels[y * ScreenWidth + x] = rgb(150, 150, 150);

        const float sx = (FilmWidth * 0.5f) - (x + 0.5f) * (FilmWidth / ScreenWidth);
        const float a = player.a + atan2(sx, FocalLength);

        const float MaxDist = 1000.0f;
        float hx, hy;
        if (cast_ray(
                player.x, player.y,
                player.x + MaxDist * cos(a),
                player.y + MaxDist * sin(a),
                &hx, &hy))
        {
            const float dx = hx - player.x;
            const float dy = hy - player.y;
            const float d = sqrt(dx * dx + dy * dy) * cos(a - player.a);
            const float h = FocalLength * WallHeight / d;

            const size_t hhy = static_cast<size_t>(h / FilmHeight * ScreenHeight) / 2;
            const size_t midy = ScreenHeight / 2;
            const size_t starty = hhy <= midy ? midy - hhy : 0;
            const size_t endy = midy + hhy <= ScreenHeight - 1 ? midy + hhy : ScreenHeight - 1;

            const uint8_t shade = 220 - static_cast<uint8_t>(min(d / 8.0f * 255.0f, 200.0f));
            for (size_t y = starty; y < endy; ++y)
                pixels[y * ScreenWidth + x] = rgb(shade, shade, shade);
        }
    }
}

extern "C" int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        cerr << "SDL_Init Error: " << SDL_GetError() << endl;
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow(
            "Wolf Fizzle",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(ScreenWidth),
            static_cast<int>(ScreenHeight),
            SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
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
        cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
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

    while (true)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
              case SDL_QUIT:
              case SDLK_ESCAPE:
                SDL_Quit();
                return 0;

              case SDL_KEYDOWN:
                switch (e.key.keysym.sym)
                {
                  case SDLK_ESCAPE:
                    SDL_Quit();
                    return 0;

                  case SDLK_r:
                    init();
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
}
