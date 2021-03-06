#include "glout.h"

#include "graphics.h"

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <math.h>

#include "message.h"

static int extGL_ARB_NPOT;
static int extGL_ARB_TEX_RECT;

extern int filteringType;

static FillType bgType = FT_FLAT;
static GradientType bgGradient;
static Color bgColor1;
static Color bgColor2;
static Color fadeColor;

int screenWidth;
int screenHeight;

static int queryExtenstion(char *extName)
{
    char *p = (char*)glGetString(GL_EXTENSIONS);
    char *end;
    int extNameLen;

    extNameLen = strlen(extName);
    end = p + strlen(p);

    while (p < end)
    {
        int n = strcspn(p, " ");
        if ((extNameLen == n) && (strncmp(extName, p, n) == 0))
        {
            message_OutEx("Found extenstion '%s'.\n", extName);
            return 1;
        }

        p += (n + 1);
    }

    return 0;
}


void gloutInitSubsystem(int setScreenWidth, int setScreenHeight)
{
    screenWidth = setScreenWidth;
    screenHeight = setScreenHeight;

    extGL_ARB_NPOT = queryExtenstion("GL_ARB_texture_non_power_of_two");
    extGL_ARB_TEX_RECT = queryExtenstion("GL_ARB_texture_rectangle");

    if(filteringType == FILTER_MIPMAP)
    {
        extGL_ARB_NPOT = 1;
        message_Warning("Filtering set to 'mipmapping'. All sprites will be resized to nearest power of two dimensions.\n");
    }
    else
    {
        if(!extGL_ARB_NPOT && extGL_ARB_TEX_RECT)
        {
            message_Warning("Extension 'GL_ARB_texture_non_power_of_two' not found.\n");
            message_Warning("Using 'GL_ARB_texture_rectangle' instead.\n");
        }

        if(!extGL_ARB_NPOT && !extGL_ARB_TEX_RECT)
            message_CriticalError("No extensions allowing non-power of two textures found.\nGo buy a new graphics card.\n");
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_TEXTURE_2D);

    if(!extGL_ARB_NPOT)
        glEnable(GL_TEXTURE_RECTANGLE_ARB);

    glEnable(GL_VERTEX_ARRAY);
    glEnable(GL_COLOR_ARRAY);
    glEnable(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_PROJECTION);

    glLoadIdentity();

    double scaledHeight = (double)screenWidth / (double)_SCREEN_WIDTH * (double)_SCREEN_HEIGHT;

    glOrtho(0, _SCREEN_WIDTH, _SCREEN_HEIGHT, 0, -1, 1);
    glViewport(0, (screenHeight - (int)scaledHeight) / 2, screenWidth, (int)scaledHeight);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    glPushMatrix();

    glDisable(GL_LIGHTING);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

}

void gloutSetBackground(FillType bt, Color mainColor, Color auxColor, GradientType gt)
{
    bgType = bt;
    bgGradient = gt;
    bgColor1 = mainColor;
    bgColor2 = auxColor;

    if(bt == FT_FLAT)
        glClearColor(bgColor1.r, bgColor1.g, bgColor1.b, bgColor1.a);
}

void gloutClearBuffer()
{
    Point screenLTCorner = {0, 0};
    Point screenBRCorner = {_SCREEN_WIDTH, _SCREEN_HEIGHT};

    glClear(GL_COLOR_BUFFER_BIT);
    if(bgType == FT_GRADIENT)
        gloutDrawRectangle(screenLTCorner, screenBRCorner, FT_GRADIENT, bgGradient, bgColor1, bgColor2);

    glLoadIdentity();
}

void gloutBlitBuffer()
{
    Point screenLTCorner = {0, 0};
    Point screenBRCorner = {_SCREEN_WIDTH, _SCREEN_HEIGHT};

    if(fadeColor.a != 0.0)
        gloutDrawRectangle(screenLTCorner, screenBRCorner, FT_FLAT, GT_NONE, fadeColor, bgColor2);

    SDL_GL_SwapBuffers();
}

void gloutSetFadeColor(Color fcolor)
{
    fadeColor = fcolor;
}

BitmapId gloutLoadBitmap(const char* file, int *w, int *h)
{

    SDL_Surface* surface = IMG_Load(file);
    GLuint texture = 0;

    if(!surface)
    {
        message_CriticalErrorEx("SDL Error: '%s'\n", SDL_GetError());
        return 0;
    }

    *w = surface->w;
    *h = surface->h;

    GLenum target;

    if(extGL_ARB_NPOT)
        target = GL_TEXTURE_2D;
    else
        target = GL_TEXTURE_RECTANGLE_ARB;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);

    glBindTexture(target, texture);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if(filteringType == FILTER_MIPMAP)
    {
        glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if(filteringType == FILTER_LINEAR)
    {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if(filteringType == FILTER_NEAREST)
    {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glTexParameterf(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_PixelFormat *format = surface->format;


    if (format->Amask)
    {
        if(filteringType == FILTER_MIPMAP)
            gluBuild2DMipmaps(target, 4, surface->w, surface->h, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
        else
            glTexImage2D(target, 0, 4, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    }
    else
    {
        if(filteringType == FILTER_MIPMAP)
            gluBuild2DMipmaps(target, 3, surface->w, surface->h, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
        else
            glTexImage2D(target, 0, 3, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
    }



    SDL_FreeSurface(surface);
    return texture;
}

void gloutDrawRectangle(Point leftTop, Point rightBottom, FillType ft, GradientType gt, Color mainColor, Color auxColor)
{
    GLfloat vertices[8] = {
        (float)leftTop.x, (float)leftTop.y,
        rightBottom.x, (float)leftTop.y,
        rightBottom.x, rightBottom.y,
        (float)leftTop.x, rightBottom.y
    };

    GLuint indices[4] = {0, 1, 2, 3};
    GLfloat colors[16];

    if(ft == FT_FLAT)
    {
        int i;
        for(i = 0; i < 4; ++i)
        {
            colors[i * 4 + 0] = mainColor.r;
            colors[i * 4 + 1] = mainColor.g;
            colors[i * 4 + 2] = mainColor.b;
            colors[i * 4 + 3] = mainColor.a;
        }
    }

    if(ft == FT_GRADIENT)
    {
        switch(gt)
        {
                        case GT_V:

            memcpy(colors, &mainColor, sizeof(float) * 4);
            memcpy(colors + 4, &mainColor, sizeof(float) * 4);
            memcpy(colors + 8, &auxColor, sizeof(float) * 4);
            memcpy(colors + 12, &auxColor, sizeof(float) * 4);
            break;

                        case GT_H:

            memcpy(colors, &mainColor, sizeof(float) * 4);
            memcpy(colors + 4, &auxColor, sizeof(float) * 4);
            memcpy(colors + 8, &auxColor, sizeof(float) * 4);
            memcpy(colors + 12, &mainColor, sizeof(float) * 4);

            break;

                        default:;
                        }
    }

    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_FLOAT, 0, colors);

    if(extGL_ARB_NPOT)
        glBindTexture(GL_TEXTURE_2D, 0);
    else
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

    glDisable(GL_TEXTURE_COORD_ARRAY);
    glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, indices);
    glEnable(GL_TEXTURE_COORD_ARRAY);

}

void gloutBlitBitmap(BitmapId textureID, Transformations *transfs)
{
    IntPoint lt = {0, 0};
    gloutBlitPartBitmap(textureID, transfs, &lt, &transfs->size);

}

void gloutBlitPartBitmap(BitmapId textureID, Transformations *transfs, IntPoint *leftTop, IntPoint *partSize)
{

    float hw = (float)partSize->x * transfs->scale.x / 2.0;
    float hh = (float)partSize->y * transfs->scale.y / 2.0;

    GLfloat vertices[8] = {
        -hw, -hh,
        hw, -hh,
        hw, hh,
        -hw, hh
    };

    GLuint indices[4] = {0, 1, 2, 3};
    GLfloat colors[16] = {
        1, 1, 1, transfs->opacity,
        1, 1, 1, transfs->opacity,
        1, 1, 1, transfs->opacity,
        1, 1, 1, transfs->opacity
    };

    float mx = (float)transfs->size.x - 1.0f;
    float my = (float)transfs->size.y - 1.0f;
    Point lt = {(float)leftTop->x / mx, (float)leftTop->y / my};
    Point rb = {((float)leftTop->x + (float)partSize->x - 1.0f) / mx, ((float)leftTop->y + (float)partSize->y - 1.0f) / my};

    GLfloat uvsf[8] = {
        lt.x, lt.y,
        rb.x, lt.y,
        rb.x, rb.y,
        lt.x, rb.y
    };
    
    GLint uvsi[8] = {
        leftTop->x, leftTop->y,
        leftTop->x + partSize->x - 1, leftTop->y,
        leftTop->x + partSize->x - 1, leftTop->y + partSize->y - 1,
        leftTop->x, leftTop->y + partSize->y - 1
    };

    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_FLOAT, 0, colors);

    glPushMatrix();

    glTranslatef(transfs->trans.x, transfs->trans.y, 0);

    // position by upper-left corner
    glTranslatef(hw, hh, 0);

    // rotate by center
    glRotatef(transfs->angle, 0.0, 0.0, 1.0f);

    // flip if needed
    if(transfs->vflip)
        glRotatef(180.0, 1.0f, 0.0, 0.0);

    if(transfs->hflip)
        glRotatef(180.0, 0.0, 1.0f, 0.0);

    if(extGL_ARB_NPOT)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexCoordPointer(2, GL_FLOAT, 0, uvsf);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, textureID);
        glTexCoordPointer(2, GL_INT, 0, uvsi);

    }

    glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, indices);

    glPopMatrix();
}

