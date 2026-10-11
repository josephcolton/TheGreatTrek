#include "text.h"

int text_draw(SDL_Renderer *r, TTF_Font *font, const char *text, float x, float y,
              int wrap_w, SDL_Color color, TextAlign align)
{
    if (!text || !*text) {
        return 0;
    }
    /* Solid (not anti-aliased) rendering keeps the pixel font crisp when scaled up. */
    SDL_Surface *surf = wrap_w > 0 ? TTF_RenderText_Solid_Wrapped(font, text, 0, color, wrap_w)
                                   : TTF_RenderText_Solid(font, text, 0, color);
    if (!surf) {
        return 0;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    int h = surf->h;
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_FRect dst = {x, y, (float)surf->w, (float)surf->h};
        if (align == TEXT_CENTER) {
            dst.x = x - (float)surf->w / 2.0f;
        }
        SDL_RenderTexture(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_DestroySurface(surf);
    return h;
}
