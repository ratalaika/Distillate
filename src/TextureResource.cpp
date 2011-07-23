/**
 * Copyright (c) 2010 - 2011 Distillate Project
 *
 *  ______ ________________________            _____________________
 *  |     \  |  |______   |     |  |     |     |_____|   |   |______
 *  |_____/__|________|   |   __|__|_____|_____|     |   |   |______
 *
 *
 * License: BSD
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Wintermoon nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "include/TextureResource.hpp"
#include "include/Globals.hpp"

#if defined(GL_ENGINE)
#include <png.h>
#endif

NAMESPACE_BEGIN

bool PNGCheck(const std::string &filename) {
#if defined(SDL_ENGINE)
    return true;
#elif defined(GL_ENGINE)
    png_byte header[8];
    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp) return false;
    fread(header, 1, 8, fp);
    int is_png = !png_sig_cmp(header, 0, 8);
    fclose(fp);
    if (is_png) return true;
#ifdef DEBUG
    fprintf(stdout, "Not PNG File\n");
#endif
    return false;
#endif
}

TextureLoader::TextureType TextureLoader::checkTexture(Resource* r) {
    if(r && r->filename.empty()) return NONE;
    if(PNGCheck(r->filename)) return PNG_TEXTURE;
    return NONE;
}

void PNGTextureImplementation::process(Resource* r) {
    if(!r) {
        fprintf(stderr, "Null Resource detected\n");
        return;
    }

    TextureResource *texRes = static_cast<TextureResource*>(r);
#if defined(SDL_ENGINE)
    texRes->data = IMG_Load(r->filename.c_str());

    if(!texRes->data) {
        fprintf(stderr, "Null pointer %s\n", IMG_GetError());
    }

    texRes->w = texRes->data->w;
    texRes->h = texRes->data->h;
#elif defined(GL_ENGINE)
    FILE *fp = fopen(r->filename.c_str(), "rb");

    if (!fp) {
        fprintf(stderr, "Null point detected\n");
        return;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        fprintf(stderr, "Null point detected\n");
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
        fclose(fp);
        fprintf(stderr, "Null point detected\n");
        return;
    }

    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        fprintf(stderr, "Null point detected\n");
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        fprintf(stderr, "Null point detected\n");
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    int bit_depth, color_type;
    png_uint_32 twidth, theight;

    png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type, NULL, NULL, NULL);

    int width = twidth;
    int height = theight;
    png_read_update_info(png_ptr, info_ptr);

    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    png_byte *image_data = new png_byte[rowbytes * height];

    if (!image_data) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        fprintf(stderr, "Null point detected\n");
        return;
    }

    png_bytep *row_pointers = new png_bytep[height];
    if (!row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        delete[] image_data;
        fclose(fp);
        return ;
    }

    for (int i = 0; i < height; ++i)
        row_pointers[height - 1 - i] = image_data + i * rowbytes;


    glGenTextures(1, &texRes->data);
    glBindTexture(GL_TEXTURE_2D, texRes->data);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*) image_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    delete[] image_data;
    delete[] row_pointers;
    fclose(fp);
#endif

    if(texRes)
        texRes->count++;

    if(!texRes->data)
        fprintf(stderr, "Error cannot load texture\n");
}

void AutoTextureImplementation::process(Resource* r)
{
    if(!r) {
        fprintf(stderr, "Null Resource detected\n");
        return;
    }

    TextureResource *texRes = static_cast<TextureResource*>(r);
#if defined(SDL_ENGINE)
    unsigned int rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    texRes->data = SDL_CreateRGBSurface(SDL_SWSURFACE, texRes->w, texRes->h,Globals::bpp,rmask, gmask, bmask, amask);

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = texRes->w;
    rect.h = texRes->h;

    SDL_FillRect(texRes->data, &rect, texRes->color);
#endif

    if(texRes)
        texRes->count++;

    if(!texRes->data)
        fprintf(stderr, "Error cannot load texture\n");
}

NAMESPACE_END
