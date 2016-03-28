#include <SDL.h>
#include <SDL_main.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace std;

#define myassert(cond) if (!(cond)) { __debugbreak(); }

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

#if 0

const size_t MapW = 4;
const size_t MapH = 4;

// (0,0) at bottom left.
const uint8_t Map[MapW * MapH] =
{
    1, 1, 1, 1,
    1, 0, 0, 1,
    1, 0, 0, 1,
    1, 1, 1, 1
};

#else

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

#endif

uint8_t map(const int ix, const int iy)
{
    myassert(
        ix >= 0 &&
        iy >= 0 &&
        ix <= static_cast<int>(MapW) - 1 &&
        iy <= static_cast<int>(MapH) - 1);
    return
        ix >= 0 &&
        iy >= 0 &&
        ix <= static_cast<int>(MapW) - 1 &&
        iy <= static_cast<int>(MapH) - 1
            ? Map[(MapH - 1 - iy) * MapW + ix]
            : 1;
}

const float Pi = 3.1415926535897932f;
const float HFov = 90.0f * Pi / 180.0f;
const float VFov = HFov * ScreenHeight / ScreenWidth;
const float PlayerMoveSpeed = 0.03f;
const float PlayerRotateSpeed = 3.0f * Pi / 180.0f;
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
    player.x = 2.0f;
    player.y = 2.0f;
    player.a = 180.0f * Pi / 180.0f;
}

bool cast_ray(
    const float x0, const float y0,
    const float x1, const float y1,
    float* hx, float* hy)
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


        myassert(tx < ty || ty < tx);

        if (tx < ty)
        {
            x = wallx;
            y += tx * dy;

            myassert(dx != 0.0f);
            if (dx > 0.0f)
            {
                ix += 1;
                myassert(ix == static_cast<int>(x));
                myassert(ix <= static_cast<int>(MapW - 1));
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
                myassert(iy <= static_cast<int>(MapH - 1));
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

    const float Margin = 0.2f;

    const int ix = static_cast<int>(player.x);
    const int iy = static_cast<int>(player.y);
    const int nx = ix + (dx > 0.0f ? 1 : -1);
    const int ny = iy + (dy > 0.0f ? 1 : -1);

    if (map(nx, iy))
    {
        if (dx > 0.0f)
        {
            const float wallx = nx - Margin;
            if (player.x + dx >= wallx)
                player.x = wallx;
            else player.x += dx;
        }
        else if (dx < 0.0f)
        {
            const float wallx = nx + 1 + Margin;
            if (player.x + dx <= wallx)
                player.x = wallx;
            else player.x += dx;
        }
    }
    else player.x += dx;

    if (map(ix, ny))
    {
        if (dy > 0.0f)
        {
            const float wally = ny - Margin;
            if (player.y + dy >= wally)
                player.y = wally;
            else player.y += dy;
        }
        else if (dy < 0.0f)
        {
            const float wally = ny + 1 + Margin;
            if (player.y + dy <= wally)
                player.y = wally;
            else player.y += dy;
        }
    }
    else player.y += dy;
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
