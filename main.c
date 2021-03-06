/**
 * Powder Toy - Main source
 *
 * Copyright (c) 2008 - 2010 Stanislaw Skowronek.
 * Copyright (c) 2010 Simon Robertshaw
 * Copyright (c) 2010 Skresanov Savely
 * Copyright (c) 2010 Bryan Hoyle
 * Copyright (c) 2010 Nathan Cousins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL/SDL.h>
#include <bzlib.h>
#include <time.h>

#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "misc.h"
#include "font.h"
#include "defines.h"
#include "powder.h"
#include "graphics.h"
#include "version.h"
#include "http.h"
#include "md5.h"
#include "update.h"
#include "hmap.h"
#include "air.h"

char *it_msg =
    "\brThe Powder Toy\n"
    "\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\n"
    "\n"
    "\bgControl+C/V/X are Copy, Paste and cut respectively.\n"
    "\bgTo choose a material, hover over once of the icons on the right, it will show a selection of elements in that group.\n"
    "\bgPick your material from the menu using mouse left/right buttons.\n"
    "Draw freeform lines by dragging your mouse left/right button across the drawing area.\n"
    "Shift+drag will create straight lines of particles.\n"
    "Ctrl+drag will result in filled rectangles.\n"
    "Ctrl+Shift+click will flood-fill a closed area.\n"
    "Ctrl+Z will act as Undo.\n"
    "Middle click or Alt+Click to \"sample\" the particles.\n"
    "\n\boUse 'Z' for a zoom tool. Click to make the drawable zoom window stay around. Use the wheel to change the zoom strength\n"
    "Use 'S' to save parts of the window as 'stamps'.\n"
    "'L' will load the most recent stamp, 'K' shows a library of stamps you saved.\n"
    "'C' will cycle the display mode (Fire, Blob, Velocity and Pressure). The numbers 1 to 6 will do the same\n"
    "Use the mouse scroll wheel to change the tool size for particles.\n"
    "'Q' will quit the application.\n"
    "The spacebar can be used to pause physics.\n"
    "'P' will take a screenshot and save it into the current directory.\n"
    "\n"
    "\brhttp://powdertoy.co.uk/\n"
    "\bgCopyright (c) 2008-10 Stanislaw K Skowronek (\brhttp://powder.unaligned.org\bg, \bbirc.unaligned.org #wtf\bg)\n"
    "\bgCopyright (c) 2010 Simon Robertshaw (\brhttp://powdertoy.co.uk\bg, \bbirc.freenode.net #powder\bg)\n"
    "\bgCopyright (c) 2010 Skresanov Savely (Stickman)\n"
    "\bgCopyright (c) 2010 Bryan Hoyle (New elements)\n"
    "\bgCopyright (c) 2010 Nathan Cousins (New elements, small engine mods.)\n"
    "\n"
    "\bgSpecial thanks to Brian Ledbetter for maintaining ports.\n"
    "\bgTo use online features such as saving, you need to register at: \brhttp://powdertoy.co.uk/Register.html"
    ;

typedef struct
{
    int start, inc;
    pixel *vid;
} upstruc;

#ifdef BETA
char *old_ver_msg_beta = "A new beta is available - click here!";
#endif
char *old_ver_msg = "A new version is available - click here!";
float mheat = 0.0f;

int do_open = 0;
int sys_pause = 0;
int legacy_enable = 0; //Used to disable new features such as heat, will be set by commandline or save.
int death = 1, framerender = 0;
int amd = 1;



#define MAXSIGNS 16

struct sign
{
    int x,y,ju;
    char text[256];
} signs[MAXSIGNS];

/***********************************************************
 *                   AIR FLOW SIMULATOR                    *
 ***********************************************************/

int numCores = 4;

int core_count()
{
    int numCPU = 1;
#ifdef MT
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );
    numCPU = sysinfo.dwNumberOfProcessors;
#else
#ifdef MACOSX
    numCPU = 4;
#else
    numCPU = sysconf( _SC_NPROCESSORS_ONLN );
#endif
#endif

    printf("Cpus: %d\n", numCPU);
    if(numCPU>1)
        printf("Multithreading enabled\n");
    else
        printf("Multithreading disabled\n");
#endif
    return numCPU;
}

/***********************************************************
 *                   PARTICLE SIMULATOR                    *
 ***********************************************************/
int mousex, mousey = 0;  //They contain mouse position




/***********************************************************
 *                       SDL OUTPUT                        *
 ***********************************************************/

SDLMod sdl_mod;
int sdl_key, sdl_wheel, sdl_caps=0, sdl_ascii, sdl_zoom_trig=0;

#include "icon.h"
void sdl_seticon(void)
{
#ifdef WIN32
    //SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon_w32, 32, 32, 32, 128, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    //SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#else
#ifdef MACOSX
    //SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon_w32, 32, 32, 32, 128, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    //SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#else
    SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon, 16, 16, 32, 128, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#endif
#endif
}

void sdl_open(void)
{
    if(SDL_Init(SDL_INIT_VIDEO)<0)
    {
        fprintf(stderr, "Initializing SDL: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
#ifdef PIX16
    sdl_scrn=SDL_SetVideoMode(XRES*sdl_scale + BARSIZE*sdl_scale,YRES*sdl_scale + MENUSIZE*sdl_scale,16,SDL_SWSURFACE);
#else
    sdl_scrn=SDL_SetVideoMode(XRES*sdl_scale + BARSIZE*sdl_scale,YRES*sdl_scale + MENUSIZE*sdl_scale,32,SDL_SWSURFACE);
#endif
    if(!sdl_scrn)
    {
        fprintf(stderr, "Creating window: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_WM_SetCaption("The Powder Toy", "Powder Toy");
    sdl_seticon();
    SDL_EnableUNICODE(1);
    //SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}

int frame_idx=0;
void dump_frame(pixel *src, int w, int h, int pitch)
{
    char frame_name[32];
    int j,i;
    unsigned char c[3];
    FILE *f;
    sprintf(frame_name,"frame%04d.ppm",frame_idx);
    f=fopen(frame_name,"wb");
    fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(j=0; j<h; j++)
    {
        for(i=0; i<w; i++)
        {
            c[0] = PIXR(src[i]);
            c[1] = PIXG(src[i]);
            c[2] = PIXB(src[i]);
            fwrite(c,3,1,f);
        }
        src+=pitch;
    }
    fclose(f);
    frame_idx++;
}

int Z_keysym = 'z';
int sdl_poll(void)
{
    SDL_Event event;
    sdl_key=sdl_wheel=sdl_ascii=0;
    while(SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_KEYDOWN:
            sdl_key=event.key.keysym.sym;
            sdl_ascii=event.key.keysym.unicode;
            if(event.key.keysym.sym == SDLK_CAPSLOCK)
                sdl_caps = 1;
            if(event.key.keysym.unicode=='z' || event.key.keysym.unicode=='Z')
            {
                sdl_zoom_trig = 1;
                Z_keysym = event.key.keysym.sym;
            }
            if( event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_RIGHTBRACKET)
            {
                sdl_wheel++;
            }
            if( event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_LEFTBRACKET)
            {
                sdl_wheel--;
            }
            //  4
            //1 8 2
            if(event.key.keysym.sym == SDLK_RIGHT)
            {
                player[0] = (int)(player[0])|0x02;  //Go right command
            }
            if(event.key.keysym.sym == SDLK_LEFT)
            {
                player[0] = (int)(player[0])|0x01;  //Go left command
            }
            if(event.key.keysym.sym == SDLK_DOWN && ((int)(player[0])&0x08)!=0x08)
            {
                player[0] = (int)(player[0])|0x08;  //Go left command
            }
            if(event.key.keysym.sym == SDLK_UP && ((int)(player[0])&0x04)!=0x04)
            {
                player[0] = (int)(player[0])|0x04;  //Jump command
            }
            break;

        case SDL_KEYUP:
            if(event.key.keysym.sym == SDLK_CAPSLOCK)
                sdl_caps = 0;
            if(event.key.keysym.sym == Z_keysym)
                sdl_zoom_trig = 0;
            if(event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_LEFT)
            {
                player[1] = player[0];  //Saving last movement
                player[0] = (int)(player[0])&12;  //Stop command
            }
            if(event.key.keysym.sym == SDLK_UP)
            {
                player[0] = (int)(player[0])&11;
            }
            if(event.key.keysym.sym == SDLK_DOWN)
            {
                player[0] = (int)(player[0])&7;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_WHEELUP)
                sdl_wheel++;
            if(event.button.button == SDL_BUTTON_WHEELDOWN)
                sdl_wheel--;
            break;
        case SDL_QUIT:
            return 1;
        }
    }
    sdl_mod = SDL_GetModState();
    return 0;
}

/***********************************************************
 *                    STATE MANAGEMENT                     *
 ***********************************************************/

int svf_login = 0;
int svf_admin = 0;
int svf_mod = 0;
char svf_user[64] = "";
char svf_pass[64] = "";

int svf_open = 0;
int svf_own = 0;
int svf_myvote = 0;
int svf_publish = 0;
char svf_id[16] = "";
char svf_name[64] = "";
char svf_tags[256] = "";
void *svf_last = NULL;
int svf_lsize;

void *build_thumb(int *size, int bzip2)
{
    unsigned char *d=calloc(1,XRES*YRES), *c;
    int i,j,x,y;
    for(i=0; i<NPART; i++)
        if(parts[i].type)
        {
            x = (int)(parts[i].x+0.5f);
            y = (int)(parts[i].y+0.5f);
            if(x>=0 && x<XRES && y>=0 && y<YRES)
                d[x+y*XRES] = parts[i].type;
        }
    for(y=0; y<YRES/CELL; y++)
        for(x=0; x<XRES/CELL; x++)
            if(bmap[y][x])
                for(j=0; j<CELL; j++)
                    for(i=0; i<CELL; i++)
                        d[x*CELL+i+(y*CELL+j)*XRES] = 0xFF;
    j = XRES*YRES;

    if(bzip2)
    {
        i = (j*101+99)/100 + 608;
        c = malloc(i);

        c[0] = 0x53;
        c[1] = 0x68;
        c[2] = 0x49;
        c[3] = 0x74;
        c[4] = PT_NUM;
        c[5] = CELL;
        c[6] = XRES/CELL;
        c[7] = YRES/CELL;

        i -= 8;

        if(BZ2_bzBuffToBuffCompress((char *)(c+8), (unsigned *)&i, (char *)d, j, 9, 0, 0) != BZ_OK)
        {
            free(d);
            free(c);
            return NULL;
        }
        free(d);
        *size = i+8;
        return c;
    }

    *size = j;
    return d;
}

int render_thumb(void *thumb, int size, int bzip2, pixel *vid_buf, int px, int py, int scl)
{
    unsigned char *d,*c=thumb;
    int i,j,x,y,a,t,r,g,b,sx,sy;

    if(bzip2)
    {
        if(size<16)
            return 1;
        if(c[3]!=0x74 || c[2]!=0x49 || c[1]!=0x68 || c[0]!=0x53)
            return 1;
        if(c[4]>PT_NUM)
            return 2;
        if(c[5]!=CELL || c[6]!=XRES/CELL || c[7]!=YRES/CELL)
            return 3;
        i = XRES*YRES;
        d = malloc(i);
        if(!d)
            return 1;

        if(BZ2_bzBuffToBuffDecompress((char *)d, (unsigned *)&i, (char *)(c+8), size-8, 0, 0))
            return 1;
        size = i;
    }
    else
        d = c;

    if(size < XRES*YRES)
    {
        if(bzip2)
            free(d);
        return 1;
    }

    sy = 0;
    for(y=0; y+scl<=YRES; y+=scl)
    {
        sx = 0;
        for(x=0; x+scl<=XRES; x+=scl)
        {
            a = 0;
            r = g = b = 0;
            for(j=0; j<scl; j++)
                for(i=0; i<scl; i++)
                {
                    t = d[(y+j)*XRES+(x+i)];
                    if(t==0xFF)
                    {
                        r += 256;
                        g += 256;
                        b += 256;
                        a += 2;
                    }
                    else if(t)
                    {
                        if(t>=PT_NUM)
                            goto corrupt;
                        r += PIXR(ptypes[t].pcolors);
                        g += PIXG(ptypes[t].pcolors);
                        b += PIXB(ptypes[t].pcolors);
                        a ++;
                    }
                }
            if(a)
            {
                a = 256/a;
                r = (r*a)>>8;
                g = (g*a)>>8;
                b = (b*a)>>8;
            }

            drawpixel(vid_buf, px+sx, py+sy, r, g, b, 255);
            sx++;
        }
        sy++;
    }

    if(bzip2)
        free(d);
    return 0;

corrupt:
    if(bzip2)
        free(d);
    return 1;
}

static char *mystrdup(char *s)
{
    char *x;
    if(s)
    {
        x = malloc(strlen(s)+1);
        strcpy(x, s);
        return x;
    }
    return s;
}

void *build_save(int *size, int x0, int y0, int w, int h)
{
    unsigned char *d=calloc(1,3*(XRES/CELL)*(YRES/CELL)+(XRES*YRES)*7+MAXSIGNS*262), *c;
    int i,j,x,y,p=0,*m=calloc(XRES*YRES, sizeof(int));
    int bx0=x0/CELL, by0=y0/CELL, bw=(w+CELL-1)/CELL, bh=(h+CELL-1)/CELL;

    // normalize coordinates
    x0 = bx0*CELL;
    y0 = by0*CELL;
    w  = bw *CELL;
    h  = bh *CELL;

    // save the required air state
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
            d[p++] = bmap[y][x];
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
            if(bmap[y][x]==4)
            {
                i = (int)(fvx[y][x]*64.0f+127.5f);
                if(i<0) i=0;
                if(i>255) i=255;
                d[p++] = i;
            }
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
            if(bmap[y][x]==4)
            {
                i = (int)(fvy[y][x]*64.0f+127.5f);
                if(i<0) i=0;
                if(i>255) i=255;
                d[p++] = i;
            }

    // save the particle map
    for(i=0; i<NPART; i++)
        if(parts[i].type)
        {
            x = (int)(parts[i].x+0.5f);
            y = (int)(parts[i].y+0.5f);
            if(x>=x0 && x<x0+w && y>=y0 && y<y0+h)
                m[(x-x0)+(y-y0)*w] = i+1;
        }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
            d[p++] = parts[i-1].type;
        else
            d[p++] = 0;
    }

    // save particle properties
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
        {
            i--;
            x = (int)(parts[i].vx*16.0f+127.5f);
            y = (int)(parts[i].vy*16.0f+127.5f);
            if(x<0) x=0;
            if(x>255) x=255;
            if(y<0) y=0;
            if(y>255) y=255;
            d[p++] = x;
            d[p++] = y;
        }
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
            d[p++] = (parts[i-1].life+3)/4;
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
        {
            unsigned char tttemp = (unsigned char)((parts[i-1].temp+(-MIN_TEMP))/((MAX_TEMP+(-MIN_TEMP))/255));
            //if(tttemp<0) tttemp=0;
            //if(tttemp>255) tttemp=255;
            d[p++] = tttemp;
        }
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i && (parts[i-1].type==PT_CLNE || parts[i-1].type==PT_SPRK || parts[i-1].type==PT_LAVA))
            d[p++] = parts[i-1].ctype;
    }

    j = 0;
    for(i=0; i<MAXSIGNS; i++)
        if(signs[i].text[0] &&
                signs[i].x>=x0 && signs[i].x<x0+w &&
                signs[i].y>=y0 && signs[i].y<y0+h)
            j++;
    d[p++] = j;
    for(i=0; i<MAXSIGNS; i++)
        if(signs[i].text[0] &&
                signs[i].x>=x0 && signs[i].x<x0+w &&
                signs[i].y>=y0 && signs[i].y<y0+h)
        {
            d[p++] = (signs[i].x-x0);
            d[p++] = (signs[i].x-x0)>>8;
            d[p++] = (signs[i].y-y0);
            d[p++] = (signs[i].y-y0)>>8;
            d[p++] = signs[i].ju;
            x = strlen(signs[i].text);
            d[p++] = x;
            memcpy(d+p, signs[i].text, x);
            p+=x;
        }

    i = (p*101+99)/100 + 612;
    c = malloc(i);
    c[0] = 0x66;
    c[1] = 0x75;
    c[2] = 0x43;
    c[3] = legacy_enable;
    c[4] = SAVE_VERSION;
    c[5] = CELL;
    c[6] = bw;
    c[7] = bh;
    c[8] = p;
    c[9] = p >> 8;
    c[10] = p >> 16;
    c[11] = p >> 24;

    i -= 12;

    if(BZ2_bzBuffToBuffCompress((char *)(c+12), (unsigned *)&i, (char *)d, p, 9, 0, 0) != BZ_OK)
    {
        free(d);
        free(c);
        return NULL;
    }
    free(d);

    *size = i+12;
    return c;
}

int parse_save(void *save, int size, int replace, int x0, int y0)
{
    unsigned char *d,*c=save;
    int i,j,k,x,y,p=0,*m=calloc(XRES*YRES, sizeof(int)), ver, pty, ty, legacy_beta=0;
    int bx0=x0/CELL, by0=y0/CELL, bw, bh, w, h;
    int fp[NPART], nf=0;

    if(size<16)
        return 1;
    if(c[2]!=0x43 || c[1]!=0x75 || c[0]!=0x66)
        return 1;
    if(c[4]>SAVE_VERSION)
        return 2;
    ver = c[4];

    if(ver<34)
    {
        legacy_enable = 1;
    }
    else
    {
        if(c[3]==1||c[3]==0)
            legacy_enable = c[3];
        else
            legacy_beta = 1;
    }

    bw = c[6];
    bh = c[7];
    if(bx0+bw > XRES/CELL)
        bx0 = XRES/CELL - bw;
    if(by0+bh > YRES/CELL)
        by0 = YRES/CELL - bh;
    if(bx0 < 0)
        bx0 = 0;
    if(by0 < 0)
        by0 = 0;

    if(c[5]!=CELL || bx0+bw>XRES/CELL || by0+bh>YRES/CELL)
        return 3;
    i = (unsigned)c[8];
    i |= ((unsigned)c[9])<<8;
    i |= ((unsigned)c[10])<<16;
    i |= ((unsigned)c[11])<<24;
    d = malloc(i);
    if(!d)
        return 1;

    if(BZ2_bzBuffToBuffDecompress((char *)d, (unsigned *)&i, (char *)(c+12), size-12, 0, 0))
        return 1;
    size = i;

    if(size < bw*bh)
        return 1;

    // normalize coordinates
    x0 = bx0*CELL;
    y0 = by0*CELL;
    w  = bw *CELL;
    h  = bh *CELL;

    if(replace)
    {
        memset(bmap, 0, sizeof(bmap));
        memset(emap, 0, sizeof(emap));
        memset(signs, 0, sizeof(signs));
        memset(parts, 0, sizeof(particle)*NPART);
        memset(pmap, 0, sizeof(pmap));
        memset(vx, 0, sizeof(vx));
        memset(vy, 0, sizeof(vy));
        memset(pv, 0, sizeof(pv));
    }

    // make a catalog of free parts
    memset(pmap, 0, sizeof(pmap));
    for(i=0; i<NPART; i++)
        if(parts[i].type)
        {
            x = (int)(parts[i].x+0.5f);
            y = (int)(parts[i].y+0.5f);
            pmap[y][x] = (i<<8)|1;
        }
        else
            fp[nf++] = i;

    // load the required air state
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
        {
            if(d[p])
                bmap[y][x] = d[p];
            p++;
        }
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
            if(d[(y-by0)*bw+(x-bx0)]==4)
            {
                if(p >= size)
                    goto corrupt;
                fvx[y][x] = (d[p++]-127.0f)/64.0f;
            }
    for(y=by0; y<by0+bh; y++)
        for(x=bx0; x<bx0+bw; x++)
            if(d[(y-by0)*bw+(x-bx0)]==4)
            {
                if(p >= size)
                    goto corrupt;
                fvy[y][x] = (d[p++]-127.0f)/64.0f;
            }

    // load the particle map
    i = 0;
    pty = p;
    for(y=y0; y<y0+h; y++)
        for(x=x0; x<x0+w; x++)
        {
            if(p >= size)
                goto corrupt;
            j=d[p++];
            if(j >= PT_NUM)
                goto corrupt;
            if(j && !(isplayer == 1 && j==PT_STKM))
            {
                if(pmap[y][x])
                {
                    k = pmap[y][x]>>8;
                    parts[k].type = j;
                    parts[k].x = (float)x;
                    parts[k].y = (float)y;
                    m[(x-x0)+(y-y0)*w] = k+1;
                }
                else if(i < nf)
                {
                    parts[fp[i]].type = j;
                    parts[fp[i]].x = (float)x;
                    parts[fp[i]].y = (float)y;
                    m[(x-x0)+(y-y0)*w] = fp[i]+1;
                    i++;
                }
                else
                    m[(x-x0)+(y-y0)*w] = NPART+1;
            }
        }

    // load particle properties
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
        {
            i--;
            if(p+1 >= size)
                goto corrupt;
            if(i < NPART)
            {
                parts[i].vx = (d[p++]-127.0f)/16.0f;
                parts[i].vy = (d[p++]-127.0f)/16.0f;
                if(parts[i].type == PT_STKM)
                {
                    player[2] = PT_DUST;

                    player[3] = parts[i].x-1;  //Setting legs positions
                    player[4] = parts[i].y+6;
                    player[5] = parts[i].x-1;
                    player[6] = parts[i].y+6;

                    player[7] = parts[i].x-3;
                    player[8] = parts[i].y+12;
                    player[9] = parts[i].x-3;
                    player[10] = parts[i].y+12;

                    player[11] = parts[i].x+1;
                    player[12] = parts[i].y+6;
                    player[13] = parts[i].x+1;
                    player[14] = parts[i].y+6;

                    player[15] = parts[i].x+3;
                    player[16] = parts[i].y+12;
                    player[17] = parts[i].x+3;
                    player[18] = parts[i].y+12;

                }
            }
            else
                p += 2;
        }
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        if(i)
        {
            if(p >= size)
                goto corrupt;
            if(i <= NPART)
                parts[i-1].life = d[p++]*4;
            else
                p++;
        }
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        ty = d[pty+j];
        if(i)
        {
            if(ver>=34&&legacy_beta==0)
            {
                if(p >= size)
                {
                    goto corrupt;
                }
                if(i <= NPART)
                {
                    parts[i-1].temp = (d[p++]*((MAX_TEMP+(-MIN_TEMP))/255))+MIN_TEMP;
                }
                else
                {
                    p++;
                }
            }
            else
            {
                parts[i-1].temp = ptypes[parts[i-1].type].heat;
            }
        }
    }
    for(j=0; j<w*h; j++)
    {
        i = m[j];
        ty = d[pty+j];
        if(i && (ty==PT_CLNE || (ty==PT_SPRK && ver>=21) || (ty==PT_LAVA && ver>=34)))
        {
            if(p >= size)
                goto corrupt;
            if(i <= NPART)
                parts[i-1].ctype = d[p++];
            else
                p++;
        }
    }

    if(p >= size)
        goto version1;
    j = d[p++];
    for(i=0; i<j; i++)
    {
        if(p+6 > size)
            goto corrupt;
        for(k=0; k<MAXSIGNS; k++)
            if(!signs[k].text[0])
                break;
        x = d[p++];
        x |= ((unsigned)d[p++])<<8;
        if(k<MAXSIGNS)
            signs[k].x = x+x0;
        x = d[p++];
        x |= ((unsigned)d[p++])<<8;
        if(k<MAXSIGNS)
            signs[k].y = x+y0;
        x = d[p++];
        if(k<MAXSIGNS)
            signs[k].ju = x;
        x = d[p++];
        if(p+x > size)
            goto corrupt;
        if(k<MAXSIGNS)
        {
            memcpy(signs[k].text, d+p, x);
            signs[k].text[x] = 0;
        }
        p += x;
    }

version1:
    free(d);

    return 0;

corrupt:
    if(replace)
    {
        legacy_enable = 0;
        memset(signs, 0, sizeof(signs));
        memset(parts, 0, sizeof(particle)*NPART);
        memset(bmap, 0, sizeof(bmap));
    }
    return 1;
}

pixel *prerender_save(void *save, int size, int *width, int *height)
{
    unsigned char *d,*c=save;
    int i,j,k,x,y,rx,ry,p=0;
    int bw,bh,w,h;
    pixel *fb;

    if(size<16)
        return NULL;
    if(c[2]!=0x43 || c[1]!=0x75 || c[0]!=0x66)
        return NULL;
    if(c[4]>SAVE_VERSION)
        return NULL;

    bw = c[6];
    bh = c[7];
    w = bw*CELL;
    h = bh*CELL;

    if(c[5]!=CELL)
        return NULL;

    i = (unsigned)c[8];
    i |= ((unsigned)c[9])<<8;
    i |= ((unsigned)c[10])<<16;
    i |= ((unsigned)c[11])<<24;
    d = malloc(i);
    if(!d)
        return NULL;
    fb = calloc(w*h, PIXELSIZE);
    if(!fb)
    {
        free(d);
        return NULL;
    }

    if(BZ2_bzBuffToBuffDecompress((char *)d, (unsigned *)&i, (char *)(c+12), size-12, 0, 0))
        goto corrupt;
    size = i;

    if(size < bw*bh)
        goto corrupt;

    k = 0;
    for(y=0; y<bh; y++)
        for(x=0; x<bw; x++)
        {
            rx = x*CELL;
            ry = y*CELL;
            switch(d[p])
            {
            case 1:
                for(j=0; j<CELL; j++)
                    for(i=0; i<CELL; i++)
                        fb[(ry+j)*w+(rx+i)] = PIXPACK(0x808080);
                break;
            case 2:
                for(j=0; j<CELL; j+=2)
                    for(i=(j>>1)&1; i<CELL; i+=2)
                        fb[(ry+j)*w+(rx+i)] = PIXPACK(0x808080);
                break;
            case 3:
                for(j=0; j<CELL; j++)
                    for(i=0; i<CELL; i++)
                        if(!(j%2) && !(i%2))
                            fb[(ry+j)*w+(rx+i)] = PIXPACK(0xC0C0C0);
                break;
            case 4:
                for(j=0; j<CELL; j+=2)
                    for(i=(j>>1)&1; i<CELL; i+=2)
                        fb[(ry+j)*w+(rx+i)] = PIXPACK(0x8080FF);
                k++;
                break;
            case 6:
                for(j=0; j<CELL; j+=2)
                    for(i=(j>>1)&1; i<CELL; i+=2)
                        fb[(ry+j)*w+(rx+i)] = PIXPACK(0xFF8080);
                break;
            case 7:
                for(j=0; j<CELL; j++)
                    for(i=0; i<CELL; i++)
                        if(!(i&j&1))
                            fb[(ry+j)*w+(rx+i)] = PIXPACK(0x808080);
                break;
            case 8:
                for(j=0; j<CELL; j++)
                    for(i=0; i<CELL; i++)
                        if(!(j%2) && !(i%2))
                            fb[(ry+j)*w+(rx+i)] = PIXPACK(0xC0C0C0);
                        else
                            fb[(ry+j)*w+(rx+i)] = PIXPACK(0x808080);
                break;
            }
            p++;
        }
    p += 2*k;
    if(p>=size)
        goto corrupt;

    for(y=0; y<h; y++)
        for(x=0; x<w; x++)
        {
            if(p >= size)
                goto corrupt;
            j=d[p++];
            if(j<PT_NUM && j>0)
            {
                if(j==PT_STKM)  //Stickman should be drawed another way
                {
                    //Stickman drawing
                    for(k=-2; k<=1; k++)
                    {
                        fb[(y-2)*w+x+k] = PIXRGB(255, 224, 178);
                        fb[(y+2)*w+x+k+1] = PIXRGB(255, 224, 178);
                        fb[(y+k+1)*w+x-2] = PIXRGB(255, 224, 178);
                        fb[(y+k)*w+x+2] = PIXRGB(255, 224, 178);
                    }
                    draw_line(fb , x, y+3, x-1, y+6, 255, 255, 255, w);
                    draw_line(fb , x-1, y+6, x-3, y+12, 255, 255, 255, w);
                    draw_line(fb , x, y+3, x+1, y+6, 255, 255, 255, w);
                    draw_line(fb , x+1, y+6, x+3, y+12, 255, 255, 255, w);
                }
                else
                    fb[y*w+x] = ptypes[j].pcolors;
            }
        }

    free(d);
    *width = w;
    *height = h;
    return fb;

corrupt:
    free(d);
    free(fb);
    return NULL;
}

/* NO, I DON'T THINK SO
 #include "fbi.h"

 pixel *render_packed_rgb(void *image, int width, int height, int cmp_size)
 {
 unsigned char *tmp;
 pixel *res;
 int i;

 tmp = malloc(width*height*3);
 if(!tmp)
 return NULL;
 res = malloc(width*height*PIXELSIZE);
 if(!res) {
 free(tmp);
 return NULL;
 }

 i = width*height*3;
 if(BZ2_bzBuffToBuffDecompress((char *)tmp, (unsigned *)&i, (char *)image, cmp_size, 0, 0)) {
 free(res);
 free(tmp);
 return NULL;
 }

 for(i=0; i<width*height; i++)
 res[i] = PIXRGB(tmp[3*i], tmp[3*i+1], tmp[3*i+2]);

 free(tmp);
 return res;
 }
 */
// stamps library

#define STAMP_X 4
#define STAMP_Y 4
#define STAMP_MAX 120

struct stamp_info
{
    char name[11];
    pixel *thumb;
    int thumb_w, thumb_h, dodelete;
} stamps[STAMP_MAX];//[STAMP_X*STAMP_Y];

int stamp_count = 0;

unsigned last_time=0, last_name=0;
void stamp_gen_name(char *fn)
{
    unsigned t=(unsigned)time(NULL);

    if(last_time!=t)
    {
        last_time=t;
        last_name=0;
    }
    else
        last_name++;

    sprintf(fn, "%08x%02x", last_time, last_name);
}

void *file_load(char *fn, int *size)
{
    FILE *f = fopen(fn, "rb");
    void *s;

    if(!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    s = malloc(*size);
    if(!s)
    {
        fclose(f);
        return NULL;
    }
    fread(s, *size, 1, f);
    fclose(f);
    return s;
}

#ifdef WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

void stamp_update(void)
{
    FILE *f;
    int i;
    f=fopen("stamps" PATH_SEP "stamps.def", "wb");
    if(!f)
        return;
    for(i=0; i<STAMP_MAX; i++)
    {
        if(!stamps[i].name[0])
            break;
        if(stamps[i].dodelete!=1)
        {
            fwrite(stamps[i].name, 1, 10, f);
        }
    }
    fclose(f);
}

#define GRID_X 5
#define GRID_Y 4
#define GRID_P 3
#define GRID_S 6
#define GRID_Z 3

void stamp_gen_thumb(int i)
{
    char fn[64];
    void *data;
    int size, factor_x, factor_y;
    pixel *tmp;

    if(stamps[i].thumb)
    {
        free(stamps[i].thumb);
        stamps[i].thumb = NULL;
    }

    sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
    data = file_load(fn, &size);

    if(data)
    {
        stamps[i].thumb = prerender_save(data, size, &(stamps[i].thumb_w), &(stamps[i].thumb_h));
        if(stamps[i].thumb && (stamps[i].thumb_w>XRES/GRID_S || stamps[i].thumb_h>YRES/GRID_S))
        {
            factor_x = ceil((float)stamps[i].thumb_w/(float)(XRES/GRID_S));
            factor_y = ceil((float)stamps[i].thumb_h/(float)(YRES/GRID_S));
            if(factor_y > factor_x)
                factor_x = factor_y;
            tmp = rescale_img(stamps[i].thumb, stamps[i].thumb_w, stamps[i].thumb_h, &(stamps[i].thumb_w), &(stamps[i].thumb_h), factor_x);
            free(stamps[i].thumb);
            stamps[i].thumb = tmp;
        }
    }

    free(data);
}

int clipboard_ready = 0;
void *clipboard_data = 0;
int clipboard_length = 0;

void stamp_save(int x, int y, int w, int h)
{
    FILE *f;
    int n;
    char fn[64], sn[16];
    void *s=build_save(&n, x, y, w, h);

#ifdef WIN32
    _mkdir("stamps");
#else
    mkdir("stamps", 0755);
#endif

    stamp_gen_name(sn);
    sprintf(fn, "stamps" PATH_SEP "%s.stm", sn);

    f = fopen(fn, "wb");
    if(!f)
        return;
    fwrite(s, n, 1, f);
    fclose(f);

    free(s);

    if(stamps[STAMP_MAX-1].thumb)
        free(stamps[STAMP_MAX-1].thumb);
    memmove(stamps+1, stamps, sizeof(struct stamp_info)*(STAMP_MAX-1));
    memset(stamps, 0, sizeof(struct stamp_info));
    if(stamp_count<STAMP_MAX)
        stamp_count++;

    strcpy(stamps[0].name, sn);
    stamp_gen_thumb(0);

    stamp_update();
}

void *stamp_load(int i, int *size)
{
    void *data;
    char fn[64];
    struct stamp_info tmp;

    if(!stamps[i].thumb || !stamps[i].name[0])
        return NULL;

    sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
    data = file_load(fn, size);
    if(!data)
        return NULL;

    if(i>0)
    {
        memcpy(&tmp, stamps+i, sizeof(struct stamp_info));
        memmove(stamps+1, stamps, sizeof(struct stamp_info)*i);
        memcpy(stamps, &tmp, sizeof(struct stamp_info));

        stamp_update();
    }

    return data;
}

void stamp_init(void)
{
    int i;
    FILE *f;

    memset(stamps, 0, sizeof(stamps));

    f=fopen("stamps" PATH_SEP "stamps.def", "rb");
    if(!f)
        return;
    for(i=0; i<STAMP_MAX; i++)
    {
        fread(stamps[i].name, 1, 10, f);
        if(!stamps[i].name[0])
            break;
        stamp_count++;
        stamp_gen_thumb(i);
    }
    fclose(f);
}

void del_stamp(int d)
{
    stamps[d].dodelete = 1;
    stamp_update();
    stamp_count = 0;
    stamp_init();
}

void menu_ui(pixel *vid_buf, int i, int *sl, int *sr)
{
    int b=1,bq,mx,my,h,x,y,n=0,height,width,sy,rows=0;
    pixel *old_vid=(pixel *)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
    fillrect(vid_buf, -1, -1, XRES+1, YRES+MENUSIZE, 0, 0, 0, 192);
    memcpy(old_vid, vid_buf, ((XRES+BARSIZE)*(YRES+MENUSIZE))*PIXELSIZE);

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }
    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;
        rows = ceil((float)msections[i].itemcount/16.0f);
        height = (ceil((float)msections[i].itemcount/16.0f)*18);
        width = restrict_flt(msections[i].itemcount*31, 0, 16*31);
        //clearrect(vid_buf, -1, -1, XRES+1, YRES+MENUSIZE+1);
        h = -1;
        x = XRES-BARSIZE-26;
        y = (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-(height/2)+(FONT_H/2)+1;
        sy = y;
        //clearrect(vid_buf, (XRES-BARSIZE-width)+1, y-4, width+4, height+4+rows);
        fillrect(vid_buf, (XRES-BARSIZE-width)-7, y-10, width+16, height+16+rows, 0, 0, 0, 100);
        drawrect(vid_buf, (XRES-BARSIZE-width)-7, y-10, width+16, height+16+rows, 255, 255, 255, 255);
        fillrect(vid_buf, (XRES-BARSIZE)+11, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-2, 15, FONT_H+3, 0, 0, 0, 100);
        drawrect(vid_buf, (XRES-BARSIZE)+10, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-2, 16, FONT_H+3, 255, 255, 255, 255);
        drawrect(vid_buf, (XRES-BARSIZE)+9, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-1, 1, FONT_H+1, 0, 0, 0, 255);
        if(i==SC_WALL)
        {
            for(n = 122; n<122+UI_WALLCOUNT; n++)
            {
                if(n!=SPC_AIR&&n!=SPC_HEAT&&n!=SPC_COOL&&n!=SPC_VACUUM)
                {
                    if(x-26<=60)
                    {
                        x = XRES-BARSIZE-26;
                        y += 19;
                    }
                    x -= draw_tool_xy(vid_buf, x, y, n, mwalls[n-122].colour)+5;
                    if(mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                        h = n;
                    }
                    else if(n==*sl)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    }
                    else if(n==*sr)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                    }
                }
            }
        }
        else if(i==SC_SPECIAL)
        {
            for(n = 122; n<122+UI_WALLCOUNT; n++)
            {
                if(n==SPC_AIR||n==SPC_HEAT||n==SPC_COOL||n==SPC_VACUUM)
                {
                    if(x-26<=60)
                    {
                        x = XRES-BARSIZE-26;
                        y += 19;
                    }
                    x -= draw_tool_xy(vid_buf, x, y, n, mwalls[n-122].colour)+5;
                    if(mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                        h = n;
                    }
                    else if(n==*sl)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    }
                    else if(n==*sr)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                    }
                }
            }
            for(n = 0; n<PT_NUM; n++)
            {
                if(ptypes[n].menusection==i&&ptypes[n].menu==1)
                {
                    if(x-26<=60)
                    {
                        x = XRES-BARSIZE-26;
                        y += 19;
                    }
                    x -= draw_tool_xy(vid_buf, x, y, n, ptypes[n].pcolors)+5;
                    if(mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                        h = n;
                    }
                    else if(n==*sl)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    }
                    else if(n==*sr)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                    }
                }
            }
        }
        else
        {
            for(n = 0; n<PT_NUM; n++)
            {
                if(ptypes[n].menusection==i&&ptypes[n].menu==1)
                {
                    if(x-26<=60)
                    {
                        x = XRES-BARSIZE-26;
                        y += 19;
                    }
                    x -= draw_tool_xy(vid_buf, x, y, n, ptypes[n].pcolors)+5;
                    if(mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                        h = n;
                    }
                    else if(n==*sl)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    }
                    else if(n==*sr)
                    {
                        drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                    }
                }
            }
        }

        if(h==-1)
        {
            drawtext(vid_buf, XRES-textwidth((char *)msections[i].name)-BARSIZE, sy+height+10, (char *)msections[i].name, 255, 255, 255, 255);
        }
        else if(i==SC_WALL||(i==SC_SPECIAL&&h>=122))
        {
            drawtext(vid_buf, XRES-textwidth((char *)mwalls[h-122].descs)-BARSIZE, sy+height+10, (char *)mwalls[h-122].descs, 255, 255, 255, 255);
        }
        else
        {
            drawtext(vid_buf, XRES-textwidth((char *)ptypes[h].descs)-BARSIZE, sy+height+10, (char *)ptypes[h].descs, 255, 255, 255, 255);
        }


        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));
        memcpy(vid_buf, old_vid, ((XRES+BARSIZE)*(YRES+MENUSIZE))*PIXELSIZE);
        if(!(mx>=(XRES-BARSIZE-width)-7 && my>=sy-10 && my<sy+height+9))
        {
            break;
        }

        if(b==1&&h!=-1)
        {
            *sl = h;
            break;
        }
        if(b==4&&h!=-1)
        {
            *sr = h;
            break;
        }
        //if(b==4&&h!=-1) {
        //	h = -1;
        //	break;
        //}

        if(sdl_key==SDLK_RETURN)
            break;
        if(sdl_key==SDLK_ESCAPE)
            break;
    }

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }
    //drawtext(vid_buf, XRES+2, (12*i)+2, msections[i].icon, 255, 255, 255, 255);
}

void menu_ui_v3(pixel *vid_buf, int i, int *sl, int *sr, int b, int bq, int mx, int my)
{
    int h,x,y,n=0,height,width,sy,rows=0;
    //bq = b;
    //b = SDL_GetMouseState(&mx, &my);
    mx /= sdl_scale;
    my /= sdl_scale;
    rows = ceil((float)msections[i].itemcount/16.0f);
    height = (ceil((float)msections[i].itemcount/16.0f)*18);
    width = restrict_flt(msections[i].itemcount*31, 0, 16*31);
    //clearrect(vid_buf, -1, -1, XRES+1, YRES+MENUSIZE+1);
    h = -1;
    x = XRES-BARSIZE-26;
    y = YRES+1;//(((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-(height/2)+(FONT_H/2)+1;
    sy = y;
    //clearrect(vid_buf, (XRES-BARSIZE-width)+1, y-4, width+4, height+4+rows);
    //fillrect(vid_buf, (XRES-BARSIZE-width)-7, y-10, width+16, height+16+rows, 0, 0, 0, 100);
    //drawrect(vid_buf, (XRES-BARSIZE-width)-7, y-10, width+16, height+16+rows, 255, 255, 255, 255);
    //fillrect(vid_buf, (XRES-BARSIZE)+11, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-2, 15, FONT_H+3, 0, 0, 0, 100);
    //drawrect(vid_buf, (XRES-BARSIZE)+10, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-2, 16, FONT_H+3, 255, 255, 255, 255);
    //drawrect(vid_buf, (XRES-BARSIZE)+9, (((YRES/SC_TOTAL)*i)+((YRES/SC_TOTAL)/2))-1, 1, FONT_H+1, 0, 0, 0, 255);
    if(i==SC_WALL)
    {
        for(n = 122; n<122+UI_WALLCOUNT; n++)
        {
            if(n!=SPC_AIR&&n!=SPC_HEAT&&n!=SPC_COOL&&n!=SPC_VACUUM)
            {
                if(x-26<=20)
                {
                    x = XRES-BARSIZE-26;
                    y += 19;
                }
                x -= draw_tool_xy(vid_buf, x, y, n, mwalls[n-122].colour)+5;
                if(!bq && mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    h = n;
                }
                else if(n==*sl)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                }
                else if(n==*sr)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                }
            }
        }
    }
    else if(i==SC_SPECIAL)
    {
        for(n = 122; n<122+UI_WALLCOUNT; n++)
        {
            if(n==SPC_AIR||n==SPC_HEAT||n==SPC_COOL||n==SPC_VACUUM)
            {
                if(x-26<=20)
                {
                    x = XRES-BARSIZE-26;
                    y += 19;
                }
                x -= draw_tool_xy(vid_buf, x, y, n, mwalls[n-122].colour)+5;
                if(!bq && mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    h = n;
                }
                else if(n==*sl)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                }
                else if(n==*sr)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                }
            }
        }
        for(n = 0; n<PT_NUM; n++)
        {
            if(ptypes[n].menusection==i&&ptypes[n].menu==1)
            {
                if(x-26<=20)
                {
                    x = XRES-BARSIZE-26;
                    y += 19;
                }
                x -= draw_tool_xy(vid_buf, x, y, n, ptypes[n].pcolors)+5;
                if(!bq && mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    h = n;
                }
                else if(n==*sl)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                }
                else if(n==*sr)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                }
            }
        }
    }
    else
    {
        for(n = 0; n<PT_NUM; n++)
        {
            if(ptypes[n].menusection==i&&ptypes[n].menu==1)
            {
                if(x-26<=20)
                {
                    x = XRES-BARSIZE-26;
                    y += 19;
                }
                x -= draw_tool_xy(vid_buf, x, y, n, ptypes[n].pcolors)+5;
                if(!bq && mx>=x+32 && mx<x+58 && my>=y && my< y+15)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                    h = n;
                }
                else if(n==*sl)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 255, 0, 0, 255);
                }
                else if(n==*sr)
                {
                    drawrect(vid_buf, x+30, y-1, 29, 17, 0, 0, 255, 255);
                }
            }
        }
    }

    if(h==-1)
    {
        drawtext(vid_buf, XRES-textwidth((char *)msections[i].name)-BARSIZE, sy-10, (char *)msections[i].name, 255, 255, 255, 255);
    }
    else if(i==SC_WALL||(i==SC_SPECIAL&&h>=122))
    {
        drawtext(vid_buf, XRES-textwidth((char *)mwalls[h-122].descs)-BARSIZE, sy-10, (char *)mwalls[h-122].descs, 255, 255, 255, 255);
    }
    else
    {
        drawtext(vid_buf, XRES-textwidth((char *)ptypes[h].descs)-BARSIZE, sy-10, (char *)ptypes[h].descs, 255, 255, 255, 255);
    }

    if(b==1&&h!=-1)
    {
        *sl = h;
    }
    if(b==4&&h!=-1)
    {
        *sr = h;
    }
}

int create_parts(int x, int y, int r, int c)
{
    int i, j, f = 0, u, v, oy, ox, b = 0, dw = 0; //n;

    if(c == 125)
    {
        i = x / CELL;
        j = y / CELL;
        for(v=-1; v<2; v++)
            for(u=-1; u<2; u++)
                if(i+u>=0 && i+u<XRES/CELL &&
                        j+v>=0 && j+v<YRES/CELL &&
                        bmap[j+v][i+u] == 5)
                    return 1;
        bmap[j][i] = 5;
        return 1;
    }
    //LOLOLOLOLLOLOLOLO
    if(c == 127)
    {
        b = 4;
        dw = 1;
    }
    if(c == 122)
    {
        b = 8;
        dw = 1;
    }
    if(c == 123)
    {
        b = 7;
        dw = 1;
    }
    if(c == 124)
    {
        b = 6;
        dw = 1;
    }
    if(c == 128)
    {
        b = 3;
        dw = 1;
    }
    if(c == 129)
    {
        b = 2;
        dw = 1;
    }
    if(c == 130)
    {
        b = 0;
        dw = 1;
    }
    if(c == 131)
    {
        b = 1;
        dw = 1;
    }
    if(c == 132)
    {
        b = 9;
        dw = 1;
    }
    if(c == 133)
    {
        b = 10;
        dw = 1;
    }
    if(c == 134)
    {
        b = 11;
        dw = 1;
    }
    if(c == 135)
    {
        b = 12;
        dw = 1;
    }
    if(c == 140)
    {
        b = 13;
        dw = 1;
    }
    if(c == 255)
    {
        b = 255;
        dw = 1;
    }
    if(dw==1)
    {
        r = r/CELL;
        x = x/CELL;
        y = y/CELL;
        x -= r/2;
        y -= r/2;
        for (ox=x; ox<=x+r; ox++)
        {
            for (oy=y; oy<=y+r; oy++)
            {
                if(ox>=0&&ox<XRES/CELL&&oy>=0&&oy<YRES/CELL)
                {
                    i = ox;
                    j = oy;
                    if(b==4)
                    {
                        fvx[j][i] = 0.0f;
                        fvy[j][i] = 0.0f;
                    }
                    bmap[j][i] = b;
                }
            }
        }
        return 1;
    }
    if(c == SPC_AIR || c == SPC_HEAT || c == SPC_COOL || c == SPC_VACUUM)
    {
        for(j=-r; j<=r; j++)
            for(i=-r; i<=r; i++)
                if(i*i+j*j<=r*r)
                    create_part(-1, x+i, y+j, c);
        return 1;
    }

    if(c == 0)
    {
        for(j=-r; j<=r; j++)
            for(i=-r; i<=r; i++)
                if(i*i+j*j<=r*r)
                    delete_part(x+i, y+j);
        return 1;
    }

    for(j=-r; j<=r; j++)
        for(i=-r; i<=r; i++)
            if(i*i+j*j<=r*r)
                if(create_part(-1, x+i, y+j, c)==-1)
                    f = 1;
    return !f;
}

void create_line(int x1, int y1, int x2, int y2, int r, int c)
{
    int cp=abs(y2-y1)>abs(x2-x1), x, y, dx, dy, sy;
    float e, de;
    if(cp)
    {
        y = x1;
        x1 = y1;
        y1 = y;
        y = x2;
        x2 = y2;
        y2 = y;
    }
    if(x1 > x2)
    {
        y = x1;
        x1 = x2;
        x2 = y;
        y = y1;
        y1 = y2;
        y2 = y;
    }
    dx = x2 - x1;
    dy = abs(y2 - y1);
    e = 0.0f;
    if(dx)
        de = dy/(float)dx;
    else
        de = 0.0f;
    y = y1;
    sy = (y1<y2) ? 1 : -1;
    for(x=x1; x<=x2; x++)
    {
        if(cp)
            create_parts(y, x, r, c);
        else
            create_parts(x, y, r, c);
        e += de;
        if(e >= 0.5f)
        {
            y += sy;
            if(c==135 || c==140 || c==134 || c==133 || c==132 || c==131 || c==129 || c==128 || c==127 || c==125 || c==124 || c==123 || c==122 || !r)
            {
                if(cp)
                    create_parts(y, x, r, c);
                else
                    create_parts(x, y, r, c);
            }
            e -= 1.0f;
        }
    }
}

void create_box(int x1, int y1, int x2, int y2, int c)
{
    int i, j;
    if(x1>x2)
    {
        i = x2;
        x2 = x1;
        x1 = i;
    }
    if(y1>y2)
    {
        j = y2;
        y2 = y1;
        y1 = j;
    }
    for(j=y1; j<=y2; j++)
        for(i=x1; i<=x2; i++)
            create_parts(i, j, 1, c);
}

int flood_parts(int x, int y, int c, int cm, int bm)
{
    int x1, x2, dy = (c<PT_NUM)?1:CELL;
    int co = c;
    if(c>=122&&c<=122+UI_WALLCOUNT)
    {
        c = c-100;
    }
    if(cm==-1)
    {
        if(c==0)
        {
            cm = pmap[y][x]&0xFF;
            if(!cm)
                return 0;
        }
        else
            cm = 0;
    }
    if(bm==-1)
    {
        if(c==30)
        {
            bm = bmap[y/CELL][x/CELL];
            if(!bm)
                return 0;
            if(bm==1)
                cm = 0xFF;
        }
        else
            bm = 0;
    }

    if((pmap[y][x]&0xFF)!=cm || bmap[y/CELL][x/CELL]!=bm)
        return 1;

    // go left as far as possible
    x1 = x2 = x;
    while(x1>=CELL)
    {
        if((pmap[y][x1-1]&0xFF)!=cm || bmap[y/CELL][(x1-1)/CELL]!=bm)
            break;
        x1--;
    }
    while(x2<XRES-CELL)
    {
        if((pmap[y][x2+1]&0xFF)!=cm || bmap[y/CELL][(x2+1)/CELL]!=bm)
            break;
        x2++;
    }

    // fill span
    for(x=x1; x<=x2; x++)
        if(!create_parts(x, y, 0, co))
            return 0;

    // fill children
    if(y>=CELL+dy)
        for(x=x1; x<=x2; x++)
            if((pmap[y-dy][x]&0xFF)==cm && bmap[(y-dy)/CELL][x/CELL]==bm)
                if(!flood_parts(x, y-dy, co, cm, bm))
                    return 0;
    if(y<YRES-CELL-dy)
        for(x=x1; x<=x2; x++)
            if((pmap[y+dy][x]&0xFF)==cm && bmap[(y+dy)/CELL][x/CELL]==bm)
                if(!flood_parts(x, y+dy, co, cm, bm))
                    return 0;
    return 1;
}

void draw_svf_ui(pixel *vid_buf)
{
    int c;

    drawtext(vid_buf, 4, YRES+(MENUSIZE-14), "\x81", 255, 255, 255, 255);
    drawrect(vid_buf, 1, YRES+(MENUSIZE-16), 16, 14, 255, 255, 255, 255);

    c = svf_open ? 255 : 128;
    drawtext(vid_buf, 23, YRES+(MENUSIZE-14), "\x91", c, c, c, 255);
    drawrect(vid_buf, 19, YRES+(MENUSIZE-16), 16, 14, c, c, c, 255);

    c = svf_login ? 255 : 128;
    drawtext(vid_buf, 40, YRES+(MENUSIZE-14), "\x82", c, c, c, 255);
    if(svf_open)
        drawtext(vid_buf, 58, YRES+(MENUSIZE-12), svf_name, c, c, c, 255);
    else
        drawtext(vid_buf, 58, YRES+(MENUSIZE-12), "[untitled simulation]", c, c, c, 255);
    drawrect(vid_buf, 37, YRES+(MENUSIZE-16), 150, 14, c, c, c, 255);
    if(svf_open && svf_own)
        drawdots(vid_buf, 55, YRES+(MENUSIZE-15), 12, c, c, c, 255);

    c = (svf_login && svf_open) ? 255 : 128;

    drawrect(vid_buf, 189, YRES+(MENUSIZE-16), 14, 14, c, c, c, 255);
    drawrect(vid_buf, 203, YRES+(MENUSIZE-16), 14, 14, c, c, c, 255);

    if(svf_myvote==1 && (svf_login && svf_open))
    {
        fillrect(vid_buf, 189, YRES+(MENUSIZE-16), 14, 14, 0, 108, 10, 255);
    }
    else if(svf_myvote==-1 && (svf_login && svf_open))
    {
        fillrect(vid_buf, 203, YRES+(MENUSIZE-16), 14, 14, 108, 10, 0, 255);
    }

    drawtext(vid_buf, 192, YRES+(MENUSIZE-12), "\xCB", 0, 187, 18, c);
    drawtext(vid_buf, 205, YRES+(MENUSIZE-14), "\xCA", 187, 40, 0, c);

    drawtext(vid_buf, 222, YRES+(MENUSIZE-15), "\x83", c, c, c, 255);
    if(svf_tags[0])
        drawtextmax(vid_buf, 240, YRES+(MENUSIZE-12), 154, svf_tags, c, c, c, 255);
    else
        drawtext(vid_buf, 240, YRES+(MENUSIZE-12), "[no tags set]", c, c, c, 255);

    drawrect(vid_buf, 219, YRES+(MENUSIZE-16), XRES+BARSIZE-380, 14, c, c, c, 255);

    drawtext(vid_buf, XRES-139+BARSIZE/*371*/, YRES+(MENUSIZE-14), "\x92", 255, 255, 255, 255);
    drawrect(vid_buf, XRES-143+BARSIZE/*367*/, YRES+(MENUSIZE-16), 16, 14, 255, 255, 255, 255);

    drawtext(vid_buf, XRES-122+BARSIZE/*388*/, YRES+(MENUSIZE-13), "\x84", 255, 255, 255, 255);
    if(svf_login)
        drawtext(vid_buf, XRES-104+BARSIZE/*406*/, YRES+(MENUSIZE-12), svf_user, 255, 255, 255, 255);
    else
        drawtext(vid_buf, XRES-104+BARSIZE/*406*/, YRES+(MENUSIZE-12), "[sign in]", 255, 255, 255, 255);
    drawrect(vid_buf, XRES-125+BARSIZE/*385*/, YRES+(MENUSIZE-16), 91, 14, 255, 255, 255, 255);

    if(sys_pause)
    {
        fillrect(vid_buf, XRES-17+BARSIZE/*493*/, YRES+(MENUSIZE-17), 16, 16, 255, 255, 255, 255);
        drawtext(vid_buf, XRES-14+BARSIZE/*496*/, YRES+(MENUSIZE-14), "\x90", 0, 0, 0, 255);
    }
    else
    {
        drawtext(vid_buf, XRES-14+BARSIZE/*496*/, YRES+(MENUSIZE-14), "\x90", 255, 255, 255, 255);
        drawrect(vid_buf, XRES-16+BARSIZE/*494*/, YRES+(MENUSIZE-16), 14, 14, 255, 255, 255, 255);
    }

    if(!legacy_enable)
    {
        fillrect(vid_buf, XRES-160+BARSIZE/*493*/, YRES+(MENUSIZE-17), 16, 16, 255, 255, 255, 255);
        drawtext(vid_buf, XRES-154+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBE", 255, 0, 0, 255);
        drawtext(vid_buf, XRES-154+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBD", 0, 0, 0, 255);
    }
    else
    {
        drawtext(vid_buf, XRES-154+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBD", 255, 255, 255, 255);
        drawrect(vid_buf, XRES-159+BARSIZE/*494*/, YRES+(MENUSIZE-16), 14, 14, 255, 255, 255, 255);
    }

    switch(cmode)
    {
    case 0:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\x98", 128, 160, 255, 255);
        break;
    case 1:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\x99", 255, 212, 32, 255);
        break;
    case 2:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\x9A", 212, 212, 212, 255);
        break;
    case 3:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\x9B", 255, 0, 0, 255);
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\x9C", 255, 255, 64, 255);
        break;
    case 4:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBF", 55, 255, 55, 255);
        break;
    case 5:
        drawtext(vid_buf, XRES-27+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBE", 255, 0, 0, 255);
        drawtext(vid_buf, XRES-27+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xBD", 255, 255, 255, 255);
        break;
    case 6:
        drawtext(vid_buf, XRES-29+BARSIZE/*481*/, YRES+(MENUSIZE-13), "\xC4", 100, 150, 255, 255);
        break;
    }
    drawrect(vid_buf, XRES-32+BARSIZE/*478*/, YRES+(MENUSIZE-16), 14, 14, 255, 255, 255, 255);

    if(svf_admin)
    {
        drawtext(vid_buf, XRES-45+BARSIZE/*463*/, YRES+(MENUSIZE-14), "\xC9", 232, 127, 35, 255);
        drawtext(vid_buf, XRES-45+BARSIZE/*463*/, YRES+(MENUSIZE-14), "\xC7", 255, 255, 255, 255);
        drawtext(vid_buf, XRES-45+BARSIZE/*463*/, YRES+(MENUSIZE-14), "\xC8", 255, 255, 255, 255);
    }
    else if(svf_mod)
    {
        drawtext(vid_buf, XRES-45+BARSIZE/*463*/, YRES+(MENUSIZE-14), "\xC9", 35, 127, 232, 255);
        drawtext(vid_buf, XRES-45+BARSIZE/*463*/, YRES+(MENUSIZE-14), "\xC7", 255, 255, 255, 255);
    }//else if(amd)
    //	drawtext(vid_buf, XRES-45/*465*/, YRES+(MENUSIZE-15), "\x97", 0, 230, 153, 255); Why is this here?
}

typedef struct ui_edit
{
    int x, y, w, nx;
    char str[256],*def;
    int focus, cursor, hide;
} ui_edit;

void ui_edit_draw(pixel *vid_buf, ui_edit *ed)
{
    int cx, i;
    char echo[256], *str;

    if(ed->hide)
    {
        for(i=0; ed->str[i]; i++)
            echo[i] = 0x8D;
        echo[i] = 0;
        str = echo;
    }
    else
        str = ed->str;

    if(ed->str[0])
    {
        drawtext(vid_buf, ed->x, ed->y, str, 255, 255, 255, 255);
        drawtext(vid_buf, ed->x+ed->w-11, ed->y-1, "\xAA", 128, 128, 128, 255);
    }
    else if(!ed->focus)
        drawtext(vid_buf, ed->x, ed->y, ed->def, 128, 128, 128, 255);
    if(ed->focus)
    {
        cx = textnwidth(str, ed->cursor);
        for(i=-3; i<9; i++)
            drawpixel(vid_buf, ed->x+cx, ed->y+i, 255, 255, 255, 255);
    }
}

char *shift_0="`1234567890-=[]\\;',./";
char *shift_1="~!@#$%^&*()_+{}|:\"<>?";

void ui_edit_process(int mx, int my, int mb, ui_edit *ed)
{
    char ch, ts[2], echo[256], *str;
    int l, i;
#ifdef RAWINPUT
    char *p;
#endif

    if(mb)
    {
        if(ed->hide)
        {
            for(i=0; ed->str[i]; i++)
                echo[i] = 0x8D;
            echo[i] = 0;
            str = echo;
        }
        else
            str = ed->str;

        if(mx>=ed->x+ed->w-11 && mx<ed->x+ed->w && my>=ed->y-5 && my<ed->y+11)
        {
            ed->focus = 1;
            ed->cursor = 0;
            ed->str[0] = 0;
        }
        else if(mx>=ed->x-ed->nx && mx<ed->x+ed->w && my>=ed->y-5 && my<ed->y+11)
        {
            ed->focus = 1;
            ed->cursor = textwidthx(str, mx-ed->x);
        }
        else
            ed->focus = 0;
    }
    if(ed->focus && sdl_key)
    {
        if(ed->hide)
        {
            for(i=0; ed->str[i]; i++)
                echo[i] = 0x8D;
            echo[i] = 0;
            str = echo;
        }
        else
            str = ed->str;

        l = strlen(ed->str);
        switch(sdl_key)
        {
        case SDLK_HOME:
            ed->cursor = 0;
            break;
        case SDLK_END:
            ed->cursor = l;
            break;
        case SDLK_LEFT:
            if(ed->cursor > 0)
                ed->cursor --;
            break;
        case SDLK_RIGHT:
            if(ed->cursor < l)
                ed->cursor ++;
            break;
        case SDLK_DELETE:
            if(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
                ed->str[ed->cursor] = 0;
            else if(ed->cursor < l)
                memmove(ed->str+ed->cursor, ed->str+ed->cursor+1, l-ed->cursor);
            break;
        case SDLK_BACKSPACE:
            if(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
            {
                if(ed->cursor > 0)
                    memmove(ed->str, ed->str+ed->cursor, l-ed->cursor+1);
                ed->cursor = 0;
            }
            else if(ed->cursor > 0)
            {
                ed->cursor--;
                memmove(ed->str+ed->cursor, ed->str+ed->cursor+1, l-ed->cursor);
            }
            break;
        default:
#ifdef RAWINPUT
            if(sdl_key>=SDLK_SPACE && sdl_key<=SDLK_z && l<255)
            {
                ch = sdl_key;
                if((sdl_mod & (KMOD_LSHIFT|KMOD_RSHIFT|KMOD_CAPS)))
                {
                    if(ch>='a' && ch<='z')
                        ch &= ~0x20;
                    p = strchr(shift_0, ch);
                    if(p)
                        ch = shift_1[p-shift_0];
                }
                ts[0]=ed->hide?0x8D:ch;
                ts[1]=0;
                if(textwidth(str)+textwidth(ts) > ed->w-14)
                    break;
                memmove(ed->str+ed->cursor+1, ed->str+ed->cursor, l+1-ed->cursor);
                ed->str[ed->cursor] = ch;
                ed->cursor++;
            }
#else
            if(sdl_ascii>=' ' && sdl_ascii<127)
            {
                ch = sdl_ascii;
                ts[0]=ed->hide?0x8D:ch;
                ts[1]=0;
                if(textwidth(str)+textwidth(ts) > ed->w-14)
                    break;
                memmove(ed->str+ed->cursor+1, ed->str+ed->cursor, l+1-ed->cursor);
                ed->str[ed->cursor] = ch;
                ed->cursor++;
            }
#endif
            break;
        }
    }
}

typedef struct ui_checkbox
{
    int x, y;
    int focus, checked;
} ui_checkbox;

void ui_checkbox_draw(pixel *vid_buf, ui_checkbox *ed)
{
    int w = 12;
    if(ed->checked)
    {
        drawtext(vid_buf, ed->x+2, ed->y+2, "\xCF", 128, 128, 128, 255);
    }
    if(ed->focus)
    {
        drawrect(vid_buf, ed->x, ed->y, w, w, 255, 255, 255, 255);
    }
    else
    {
        drawrect(vid_buf, ed->x, ed->y, w, w, 128, 128, 128, 255);
    }
}

void ui_checkbox_process(int mx, int my, int mb, int mbq, ui_checkbox *ed)
{
    int w = 12;

    if(mb && !mbq)
    {
        if(mx>=ed->x && mx<=ed->x+w && my>=ed->y && my<=ed->y+w)
        {
            ed->checked = (ed->checked)?0:1;
        }
    }
    else
    {
        if(mx>=ed->x && mx<=ed->x+w && my>=ed->y && my<=ed->y+w)
        {
            ed->focus = 1;
        }
        else
        {
            ed->focus = 0;
        }
    }
}

void error_ui(pixel *vid_buf, int err, char *txt)
{
    int x0=(XRES-240)/2,y0=(YRES-MENUSIZE)/2,b=1,bq,mx,my;
    char *msg;

    msg = malloc(strlen(txt)+16);
    if(err)
        sprintf(msg, "%03d %s", err, txt);
    else
        sprintf(msg, "%s", txt);

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        clearrect(vid_buf, x0-2, y0-2, 244, 64);
        drawrect(vid_buf, x0, y0, 240, 60, 192, 192, 192, 255);
        if(err)
            drawtext(vid_buf, x0+8, y0+8, "HTTP error:", 255, 64, 32, 255);
        else
            drawtext(vid_buf, x0+8, y0+8, "Error:", 255, 64, 32, 255);
        drawtext(vid_buf, x0+8, y0+26, msg, 255, 255, 255, 255);
        drawtext(vid_buf, x0+5, y0+49, "Dismiss", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+44, 240, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        if(b && !bq && mx>=x0 && mx<x0+240 && my>=y0+44 && my<=y0+60)
            break;

        if(sdl_key==SDLK_RETURN)
            break;
        if(sdl_key==SDLK_ESCAPE)
            break;
    }

    free(msg);

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }
}

void info_ui(pixel *vid_buf, char *top, char *txt)
{
    int x0=(XRES-240)/2,y0=(YRES-MENUSIZE)/2,b=1,bq,mx,my;

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        clearrect(vid_buf, x0-2, y0-2, 244, 64);
        drawrect(vid_buf, x0, y0, 240, 60, 192, 192, 192, 255);
        drawtext(vid_buf, x0+8, y0+8, top, 160, 160, 255, 255);
        drawtext(vid_buf, x0+8, y0+26, txt, 255, 255, 255, 255);
        drawtext(vid_buf, x0+5, y0+49, "OK", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+44, 240, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        if(b && !bq && mx>=x0 && mx<x0+240 && my>=y0+44 && my<=y0+60)
            break;

        if(sdl_key==SDLK_RETURN)
            break;
        if(sdl_key==SDLK_ESCAPE)
            break;
    }

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }
}

void info_box(pixel *vid_buf, char *msg)
{
    int w = textwidth(msg)+16;
    int x0=(XRES-w)/2,y0=(YRES-24)/2;

    clearrect(vid_buf, x0-2, y0-2, w+4, 28);
    drawrect(vid_buf, x0, y0, w, 24, 192, 192, 192, 255);
    drawtext(vid_buf, x0+8, y0+8, msg, 192, 192, 240, 255);
    sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));
}

int confirm_ui(pixel *vid_buf, char *top, char *msg, char *btn)
{
    int x0=(XRES-240)/2,y0=(YRES-MENUSIZE)/2,b=1,bq,mx,my;
    int ret = 0;

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        clearrect(vid_buf, x0-2, y0-2, 244, 64);
        drawrect(vid_buf, x0, y0, 240, 60, 192, 192, 192, 255);
        drawtext(vid_buf, x0+8, y0+8, top, 255, 216, 32, 255);
        drawtext(vid_buf, x0+8, y0+26, msg, 255, 255, 255, 255);
        drawtext(vid_buf, x0+5, y0+49, "Cancel", 255, 255, 255, 255);
        drawtext(vid_buf, x0+165, y0+49, btn, 255, 216, 32, 255);
        drawrect(vid_buf, x0, y0+44, 160, 16, 192, 192, 192, 255);
        drawrect(vid_buf, x0+160, y0+44, 80, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        if(b && !bq && mx>=x0+160 && mx<x0+240 && my>=y0+44 && my<=y0+60)
        {
            ret = 1;
            break;
        }
        if(b && !bq && mx>=x0 && mx<x0+160 && my>=y0+44 && my<=y0+60)
            break;

        if(sdl_key==SDLK_RETURN)
        {
            ret = 1;
            break;
        }
        if(sdl_key==SDLK_ESCAPE)
            break;
    }

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    return ret;
}

int execute_tagop(pixel *vid_buf, char *op, char *tag)
{
    int status;
    char *result;

    char *names[] = {"ID", "Tag", NULL};
    char *parts[2];

    char *uri = malloc(strlen(SERVER)+strlen(op)+36);
    sprintf(uri, "http://" SERVER "/Tag.api?Op=%s", op);

    parts[0] = svf_id;
    parts[1] = tag;

    result = http_multipart_post(
                 uri,
                 names, parts, NULL,
                 svf_user, svf_pass,
                 &status, NULL);

    free(uri);

    if(status!=200)
    {
        error_ui(vid_buf, status, http_ret_text(status));
        if(result)
            free(result);
        return 1;
    }
    if(result && strncmp(result, "OK", 2))
    {
        error_ui(vid_buf, 0, result);
        free(result);
        return 1;
    }

    if(result[2])
    {
        strncpy(svf_tags, result+3, 255);
        svf_id[15] = 0;
    }

    if(result)
        free(result);

    return 0;
}

struct strlist
{
    char *str;
    struct strlist *next;
};

void strlist_add(struct strlist **list, char *str)
{
    struct strlist *item = malloc(sizeof(struct strlist));
    item->str = mystrdup(str);
    item->next = *list;
    *list = item;
}

int strlist_find(struct strlist **list, char *str)
{
    struct strlist *item;
    for(item=*list; item; item=item->next)
        if(!strcmp(item->str, str))
            return 1;
    return 0;
}

void strlist_free(struct strlist **list)
{
    struct strlist *item;
    while(*list)
    {
        item = *list;
        *list = (*list)->next;
        free(item);
    }
}

void tag_list_ui(pixel *vid_buf)
{
    int y,d,x0=(XRES-192)/2,y0=(YRES-256)/2,b=1,bq,mx,my,vp,vn;
    char *p,*q,s;
    char *tag=NULL, *op=NULL;
    ui_edit ed;
    struct strlist *vote=NULL,*down=NULL;

    ed.x = x0+25;
    ed.y = y0+221;
    ed.w = 158;
    ed.nx = 1;
    ed.def = "[new tag]";
    ed.focus = 0;
    ed.hide = 0;
    ed.cursor = 0;
    strcpy(ed.str, "");

    fillrect(vid_buf, -1, -1, XRES, YRES+MENUSIZE, 0, 0, 0, 192);
    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        op = tag = NULL;

        drawrect(vid_buf, x0, y0, 192, 256, 192, 192, 192, 255);
        clearrect(vid_buf, x0, y0, 192, 256);
        drawtext(vid_buf, x0+8, y0+8, "Current tags:", 255, 255, 255, 255);
        p = svf_tags;
        s = svf_tags[0] ? ' ' : 0;
        y = 36 + y0;
        while(s)
        {
            q = strchr(p, ' ');
            if(!q)
                q = p+strlen(p);
            s = *q;
            *q = 0;
            if(svf_own || svf_admin || svf_mod)
            {
                drawtext(vid_buf, x0+20, y-1, "\x86", 160, 48, 32, 255);
                drawtext(vid_buf, x0+20, y-1, "\x85", 255, 255, 255, 255);
                d = 14;
                if(b && !bq && mx>=x0+18 && mx<x0+32 && my>=y-2 && my<y+12)
                {
                    op = "delete";
                    tag = mystrdup(p);
                }
            }
            else
                d = 0;
            vp = strlist_find(&vote, p);
            vn = strlist_find(&down, p);
            if((!vp && !vn && !svf_own) || svf_admin || svf_mod)
            {
                drawtext(vid_buf, x0+d+20, y-1, "\x88", 32, 144, 32, 255);
                drawtext(vid_buf, x0+d+20, y-1, "\x87", 255, 255, 255, 255);
                if(b && !bq && mx>=x0+d+18 && mx<x0+d+32 && my>=y-2 && my<y+12)
                {
                    op = "vote";
                    tag = mystrdup(p);
                    strlist_add(&vote, p);
                }
                drawtext(vid_buf, x0+d+34, y-1, "\x88", 144, 48, 32, 255);
                drawtext(vid_buf, x0+d+34, y-1, "\xA2", 255, 255, 255, 255);
                if(b && !bq && mx>=x0+d+32 && mx<x0+d+46 && my>=y-2 && my<y+12)
                {
                    op = "down";
                    tag = mystrdup(p);
                    strlist_add(&down, p);
                }
            }
            if(vp)
                drawtext(vid_buf, x0+d+48+textwidth(p), y, " - voted!", 48, 192, 48, 255);
            if(vn)
                drawtext(vid_buf, x0+d+48+textwidth(p), y, " - voted.", 192, 64, 32, 255);
            drawtext(vid_buf, x0+d+48, y, p, 192, 192, 192, 255);
            *q = s;
            p = q+1;
            y += 16;
        }
        drawtext(vid_buf, x0+11, y0+219, "\x86", 32, 144, 32, 255);
        drawtext(vid_buf, x0+11, y0+219, "\x89", 255, 255, 255, 255);
        drawrect(vid_buf, x0+8, y0+216, 176, 16, 192, 192, 192, 255);
        ui_edit_draw(vid_buf, &ed);
        drawtext(vid_buf, x0+5, y0+245, "Close", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+240, 192, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        ui_edit_process(mx, my, b, &ed);

        if(b && mx>=x0 && mx<=x0+192 && my>=y0+240 && my<y0+256)
            break;

        if(op)
        {
            d = execute_tagop(vid_buf, op, tag);
            free(tag);
            op = tag = NULL;
            if(d)
                goto finish;
        }

        if(b && !bq && mx>=x0+9 && mx<x0+23 && my>=y0+218 && my<y0+232)
        {
            d = execute_tagop(vid_buf, "add", ed.str);
            strcpy(ed.str, "");
            ed.cursor = 0;
            if(d)
                goto finish;
        }

        if(sdl_key==SDLK_RETURN)
        {
            if(!ed.focus)
                break;
            d = execute_tagop(vid_buf, "add", ed.str);
            strcpy(ed.str, "");
            ed.cursor = 0;
            if(d)
                goto finish;
        }
        if(sdl_key==SDLK_ESCAPE)
        {
            if(!ed.focus)
                break;
            strcpy(ed.str, "");
            ed.cursor = 0;
            ed.focus = 0;
        }
    }
    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }
    sdl_key = 0;

finish:
    strlist_free(&vote);
}

int save_name_ui(pixel *vid_buf)
{
    int x0=(XRES-192)/2,y0=(YRES-68-YRES/4)/2,b=1,bq,mx,my,ths,nd=0;
    void *th;
    ui_edit ed;
    ui_checkbox cb;

    th = build_thumb(&ths, 0);

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    ed.x = x0+25;
    ed.y = y0+25;
    ed.w = 158;
    ed.nx = 1;
    ed.def = "[simulation name]";
    ed.focus = 1;
    ed.hide = 0;
    ed.cursor = strlen(svf_name);
    strcpy(ed.str, svf_name);

    cb.x = x0+10;
    cb.y = y0+53+YRES/4;
    cb.focus = 0;
    cb.checked = svf_publish;

    fillrect(vid_buf, -1, -1, XRES+BARSIZE, YRES+MENUSIZE, 0, 0, 0, 192);
    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        drawrect(vid_buf, x0, y0, 192, 90+YRES/4, 192, 192, 192, 255);
        clearrect(vid_buf, x0, y0, 192, 90+YRES/4);
        drawtext(vid_buf, x0+8, y0+8, "New simulation name:", 255, 255, 255, 255);
        drawtext(vid_buf, x0+10, y0+23, "\x82", 192, 192, 192, 255);
        drawrect(vid_buf, x0+8, y0+20, 176, 16, 192, 192, 192, 255);

        ui_edit_draw(vid_buf, &ed);

        drawrect(vid_buf, x0+(192-XRES/4)/2-2, y0+42, XRES/4+3, YRES/4+3, 128, 128, 128, 255);
        render_thumb(th, ths, 0, vid_buf, x0+(192-XRES/4)/2, y0+44, 4);

        ui_checkbox_draw(vid_buf, &cb);
        drawtext(vid_buf, x0+34, y0+50+YRES/4, "Publish? (Do not publish others'\nworks without permission)", 192, 192, 192, 255);

        drawtext(vid_buf, x0+5, y0+79+YRES/4, "Save simulation", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+74+YRES/4, 192, 16, 192, 192, 192, 255);

        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        ui_edit_process(mx, my, b, &ed);
        ui_checkbox_process(mx, my, b, bq, &cb);

        if(b && !bq && ((mx>=x0+9 && mx<x0+23 && my>=y0+22 && my<y0+36) ||
                        (mx>=x0 && mx<x0+192 && my>=y0+74+YRES/4 && my<y0+90+YRES/4)))
        {
            free(th);
            if(!ed.str[0])
                return 0;
            nd = strcmp(svf_name, ed.str) || !svf_own;
            strncpy(svf_name, ed.str, 63);
            svf_name[63] = 0;
            if(nd)
            {
                strcpy(svf_id, "");
                strcpy(svf_tags, "");
            }
            svf_open = 1;
            svf_own = 1;
            svf_publish = cb.checked;
            return nd+1;
        }

        if(sdl_key==SDLK_RETURN)
        {
            free(th);
            if(!ed.str[0])
                return 0;
            nd = strcmp(svf_name, ed.str) || !svf_own;
            strncpy(svf_name, ed.str, 63);
            svf_name[63] = 0;
            if(nd)
            {
                strcpy(svf_id, "");
                strcpy(svf_tags, "");
            }
            svf_open = 1;
            svf_own = 1;
            svf_publish = cb.checked;
            return nd+1;
        }
        if(sdl_key==SDLK_ESCAPE)
        {
            if(!ed.focus)
                break;
            ed.focus = 0;
        }
    }
    free(th);
    return 0;
}

void thumb_cache_inval(char *id);

void execute_save(pixel *vid_buf)
{
    int status;
    char *result;

    char *names[] = {"Name", "Data:save.bin", "Thumb:thumb.bin", "Publish", "ID", NULL};
    char *parts[5];
    int plens[5];

    parts[0] = svf_name;
    plens[0] = strlen(svf_name);
    parts[1] = build_save(plens+1, 0, 0, XRES, YRES);
    parts[2] = build_thumb(plens+2, 1);
    parts[3] = (svf_publish==1)?"Public":"Private";
    plens[3] = strlen((svf_publish==1)?"Public":"Private");

    if(svf_id[0])
    {
        parts[4] = svf_id;
        plens[4] = strlen(svf_id);
    }
    else
        names[4] = NULL;

    result = http_multipart_post(
                 "http://" SERVER "/Save.api",
                 names, parts, plens,
                 svf_user, svf_pass,
                 &status, NULL);

    if(svf_last)
        free(svf_last);
    svf_last = parts[1];
    svf_lsize = plens[1];

    free(parts[2]);

    if(status!=200)
    {
        error_ui(vid_buf, status, http_ret_text(status));
        if(result)
            free(result);
        return;
    }
    if(result && strncmp(result, "OK", 2))
    {
        error_ui(vid_buf, 0, result);
        free(result);
        return;
    }

    if(result[2])
    {
        strncpy(svf_id, result+3, 15);
        svf_id[15] = 0;
    }

    if(!svf_id[0])
    {
        error_ui(vid_buf, 0, "No ID supplied by server");
        free(result);
        return;
    }

    thumb_cache_inval(svf_id);

    svf_own = 1;
    if(result)
        free(result);
}

void login_ui(pixel *vid_buf)
{
    int x0=(XRES-192)/2,y0=(YRES-80)/2,b=1,bq,mx,my,err;
    ui_edit ed1,ed2;
    char *res;

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    ed1.x = x0+25;
    ed1.y = y0+25;
    ed1.w = 158;
    ed1.nx = 1;
    ed1.def = "[user name]";
    ed1.focus = 1;
    ed1.hide = 0;
    ed1.cursor = strlen(svf_user);
    strcpy(ed1.str, svf_user);
    ed2.x = x0+25;
    ed2.y = y0+45;
    ed2.w = 158;
    ed2.nx = 1;
    ed2.def = "[password]";
    ed2.focus = 0;
    ed2.hide = 1;
    ed2.cursor = 0;
    strcpy(ed2.str, "");

    fillrect(vid_buf, -1, -1, XRES, YRES+MENUSIZE, 0, 0, 0, 192);
    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        drawrect(vid_buf, x0, y0, 192, 80, 192, 192, 192, 255);
        clearrect(vid_buf, x0, y0, 192, 80);
        drawtext(vid_buf, x0+8, y0+8, "Server login:", 255, 255, 255, 255);
        drawtext(vid_buf, x0+12, y0+23, "\x8B", 32, 64, 128, 255);
        drawtext(vid_buf, x0+12, y0+23, "\x8A", 255, 255, 255, 255);
        drawrect(vid_buf, x0+8, y0+20, 176, 16, 192, 192, 192, 255);
        drawtext(vid_buf, x0+11, y0+44, "\x8C", 160, 144, 32, 255);
        drawtext(vid_buf, x0+11, y0+44, "\x84", 255, 255, 255, 255);
        drawrect(vid_buf, x0+8, y0+40, 176, 16, 192, 192, 192, 255);
        ui_edit_draw(vid_buf, &ed1);
        ui_edit_draw(vid_buf, &ed2);
        drawtext(vid_buf, x0+5, y0+69, "Sign in", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+64, 192, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        ui_edit_process(mx, my, b, &ed1);
        ui_edit_process(mx, my, b, &ed2);

        if(b && !bq && mx>=x0+9 && mx<x0+23 && my>=y0+22 && my<y0+36)
            break;
        if(b && !bq && mx>=x0+9 && mx<x0+23 && my>=y0+42 && my<y0+46)
            break;
        if(b && !bq && mx>=x0 && mx<x0+192 && my>=y0+64 && my<=y0+80)
            break;

        if(sdl_key==SDLK_RETURN || sdl_key==SDLK_TAB)
        {
            if(!ed1.focus)
                break;
            ed1.focus = 0;
            ed2.focus = 1;
        }
        if(sdl_key==SDLK_ESCAPE)
        {
            if(!ed1.focus && !ed2.focus)
                return;
            ed1.focus = 0;
            ed2.focus = 0;
        }
    }

    strcpy(svf_user, ed1.str);
    md5_ascii(svf_pass, (unsigned char *)ed2.str, 0);

    res = http_multipart_post(
              "http://" SERVER "/Login.api",
              NULL, NULL, NULL,
              svf_user, svf_pass,
              &err, NULL);
    if(err != 200)
    {
        error_ui(vid_buf, err, http_ret_text(err));
        if(res)
            free(res);
        goto fail;
    }
    if(res && !strncmp(res, "OK", 2))
    {
        if(!strcmp(res, "OK ADMIN"))
        {
            svf_admin = 1;
            svf_mod = 0;
        }
        else if(!strcmp(res, "OK MOD"))
        {
            svf_admin = 0;
            svf_mod = 1;
        }
        else
        {
            svf_admin = 0;
            svf_mod = 0;
        }
        free(res);
        svf_login = 1;
        return;
    }
    if(!res)
        res = mystrdup("Unspecified Error");
    error_ui(vid_buf, 0, res);
    free(res);

fail:
    strcpy(svf_user, "");
    strcpy(svf_pass, "");
    svf_login = 0;
    svf_own = 0;
    svf_admin = 0;
    svf_mod = 0;
}

void execute_delete(pixel *vid_buf, char *id)
{
    int status;
    char *result;

    char *names[] = {"ID", NULL};
    char *parts[1];

    parts[0] = id;

    result = http_multipart_post(
                 "http://" SERVER "/Delete.api",
                 names, parts, NULL,
                 svf_user, svf_pass,
                 &status, NULL);

    if(status!=200)
    {
        error_ui(vid_buf, status, http_ret_text(status));
        if(result)
            free(result);
        return;
    }
    if(result && strncmp(result, "OK", 2))
    {
        error_ui(vid_buf, 0, result);
        free(result);
        return;
    }

    if(result)
        free(result);
}

int execute_vote(pixel *vid_buf, char *id, char *action)
{
    int status;
    char *result;

    char *names[] = {"ID", "Action", NULL};
    char *parts[2];

    parts[0] = id;
    parts[1] = action;

    result = http_multipart_post(
                 "http://" SERVER "/Vote.api",
                 names, parts, NULL,
                 svf_user, svf_pass,
                 &status, NULL);

    if(status!=200)
    {
        error_ui(vid_buf, status, http_ret_text(status));
        if(result)
            free(result);
        return 0;
    }
    if(result && strncmp(result, "OK", 2))
    {
        error_ui(vid_buf, 0, result);
        free(result);
        return 0;
    }

    if(result)
        free(result);
    return 1;
}

static char hex[] = "0123456789ABCDEF";

void strcaturl(char *dst, char *src)
{
    char *d;
    unsigned char *s;

    for(d=dst; *d; d++) ;

    for(s=(unsigned char *)src; *s; s++)
    {
        if((*s>='0' && *s<='9') ||
                (*s>='a' && *s<='z') ||
                (*s>='A' && *s<='Z'))
            *(d++) = *s;
        else
        {
            *(d++) = '%';
            *(d++) = hex[*s>>4];
            *(d++) = hex[*s&15];
        }
    }
    *d = 0;
}

#define THUMB_CACHE_SIZE 256

char *thumb_cache_id[THUMB_CACHE_SIZE];
void *thumb_cache_data[THUMB_CACHE_SIZE];
int thumb_cache_size[THUMB_CACHE_SIZE];
int thumb_cache_lru[THUMB_CACHE_SIZE];

void thumb_cache_inval(char *id)
{
    int i,j;
    for(i=0; i<THUMB_CACHE_SIZE; i++)
        if(thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
            break;
    if(i >= THUMB_CACHE_SIZE)
        return;
    free(thumb_cache_id[i]);
    free(thumb_cache_data[i]);
    thumb_cache_id[i] = NULL;
    for(j=0; j<THUMB_CACHE_SIZE; j++)
        if(thumb_cache_lru[j] > thumb_cache_lru[i])
            thumb_cache_lru[j]--;
}
void thumb_cache_add(char *id, void *thumb, int size)
{
    int i,m=-1,j=-1;
    thumb_cache_inval(id);
    for(i=0; i<THUMB_CACHE_SIZE; i++)
    {
        if(!thumb_cache_id[i])
            break;
        if(thumb_cache_lru[i] > m)
        {
            m = thumb_cache_lru[i];
            j = i;
        }
    }
    if(i >= THUMB_CACHE_SIZE)
    {
        thumb_cache_inval(thumb_cache_id[j]);
        i = j;
    }
    for(j=0; j<THUMB_CACHE_SIZE; j++)
        thumb_cache_lru[j] ++;
    thumb_cache_id[i] = mystrdup(id);
    thumb_cache_data[i] = malloc(size);
    memcpy(thumb_cache_data[i], thumb, size);
    thumb_cache_size[i] = size;
    thumb_cache_lru[i] = 0;
}
int thumb_cache_find(char *id, void **thumb, int *size)
{
    int i,j;
    for(i=0; i<THUMB_CACHE_SIZE; i++)
        if(thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
            break;
    if(i >= THUMB_CACHE_SIZE)
        return 0;
    for(j=0; j<THUMB_CACHE_SIZE; j++)
        if(thumb_cache_lru[j] < thumb_cache_lru[i])
            thumb_cache_lru[j]++;
    thumb_cache_lru[i] = 0;
    *thumb = malloc(thumb_cache_size[i]);
    *size = thumb_cache_size[i];
    memcpy(*thumb, thumb_cache_data[i], *size);
    return 1;
}

char *search_ids[GRID_X*GRID_Y];
int   search_votes[GRID_X*GRID_Y];
int   search_publish[GRID_X*GRID_Y];
int	  search_scoredown[GRID_X*GRID_Y];
int	  search_scoreup[GRID_X*GRID_Y];
char *search_names[GRID_X*GRID_Y];
char *search_owners[GRID_X*GRID_Y];
void *search_thumbs[GRID_X*GRID_Y];
int   search_thsizes[GRID_X*GRID_Y];

#define TAG_MAX 256
char *tag_names[TAG_MAX];
int tag_votes[TAG_MAX];

int search_results(char *str, int votes)
{
    int i,j;
    char *p,*q,*r,*s,*vu,*vd,*pu;

    for(i=0; i<GRID_X*GRID_Y; i++)
    {
        if(search_ids[i])
        {
            free(search_ids[i]);
            search_ids[i] = NULL;
        }
        if(search_names[i])
        {
            free(search_names[i]);
            search_names[i] = NULL;
        }
        if(search_owners[i])
        {
            free(search_owners[i]);
            search_owners[i] = NULL;
        }
        if(search_thumbs[i])
        {
            free(search_thumbs[i]);
            search_thumbs[i] = NULL;
            search_thsizes[i] = 0;
        }
    }
    for(j=0; j<TAG_MAX; j++)
        if(tag_names[j])
        {
            free(tag_names[j]);
            tag_names[j] = NULL;
        }

    if(!str || !*str)
        return 0;

    i = 0;
    j = 0;
    s = NULL;
    do_open = 0;
    while(1)
    {
        if(!*str)
            break;
        p = strchr(str, '\n');
        if(!p)
            p = str + strlen(str);
        else
            *(p++) = 0;
        if(!strncmp(str, "OPEN ", 5))
        {
            do_open = 1;
            if(i>=GRID_X*GRID_Y)
                break;
            if(votes)
            {
                pu = strchr(str+5, ' ');
                if(!pu)
                    return i;
                *(pu++) = 0;
                s = strchr(pu, ' ');
                if(!s)
                    return i;
                *(s++) = 0;
                vu = strchr(s, ' ');
                if(!vu)
                    return i;
                *(vu++) = 0;
                vd = strchr(vu, ' ');
                if(!vd)
                    return i;
                *(vd++) = 0;
                q = strchr(vd, ' ');
            }
            else
            {
                pu = strchr(str+5, ' ');
                if(!pu)
                    return i;
                *(pu++) = 0;
                vu = strchr(pu, ' ');
                if(!vu)
                    return i;
                *(vu++) = 0;
                vd = strchr(vu, ' ');
                if(!vd)
                    return i;
                *(vd++) = 0;
                q = strchr(vd, ' ');
            }
            if(!q)
                return i;
            *(q++) = 0;
            r = strchr(q, ' ');
            if(!r)
                return i;
            *(r++) = 0;
            search_ids[i] = mystrdup(str+5);

            search_publish[i] = atoi(pu);
            search_scoreup[i] = atoi(vu);
            search_scoredown[i] = atoi(vd);

            search_owners[i] = mystrdup(q);
            search_names[i] = mystrdup(r);

            if(s)
                search_votes[i] = atoi(s);
            thumb_cache_find(str, search_thumbs+i, search_thsizes+i);
            i++;
        }
        else if(!strncmp(str, "TAG ", 4))
        {
            if(j >= TAG_MAX)
            {
                str = p;
                continue;
            }
            q = strchr(str+4, ' ');
            if(!q)
            {
                str = p;
                continue;
            }
            *(q++) = 0;
            tag_names[j] = mystrdup(str+4);
            tag_votes[j] = atoi(q);
            j++;
        }
        else
        {
            if(i>=GRID_X*GRID_Y)
                break;
            if(votes)
            {
                pu = strchr(str, ' ');
                if(!pu)
                    return i;
                *(pu++) = 0;
                s = strchr(pu, ' ');
                if(!s)
                    return i;
                *(s++) = 0;
                vu = strchr(s, ' ');
                if(!vu)
                    return i;
                *(vu++) = 0;
                vd = strchr(vu, ' ');
                if(!vd)
                    return i;
                *(vd++) = 0;
                q = strchr(vd, ' ');
            }
            else
            {
                pu = strchr(str, ' ');
                if(!pu)
                    return i;
                *(pu++) = 0;
                vu = strchr(pu, ' ');
                if(!vu)
                    return i;
                *(vu++) = 0;
                vd = strchr(vu, ' ');
                if(!vd)
                    return i;
                *(vd++) = 0;
                q = strchr(vd, ' ');
            }
            if(!q)
                return i;
            *(q++) = 0;
            r = strchr(q, ' ');
            if(!r)
                return i;
            *(r++) = 0;
            search_ids[i] = mystrdup(str);

            search_publish[i] = atoi(pu);
            search_scoreup[i] = atoi(vu);
            search_scoredown[i] = atoi(vd);

            search_owners[i] = mystrdup(q);
            search_names[i] = mystrdup(r);

            if(s)
                search_votes[i] = atoi(s);
            thumb_cache_find(str, search_thumbs+i, search_thsizes+i);
            i++;
        }
        str = p;
    }
    if(*str)
        i++;
    return i;
}

#define IMGCONNS 3
#define TIMEOUT 100
#define HTTP_TIMEOUT 10

int search_own = 0;
int search_date = 0;
int search_page = 0;
char search_expr[256] = "";

int search_ui(pixel *vid_buf)
{
    int uih=0,nyu,nyd,b=1,bq,mx=0,my=0,mxq=0,myq=0,mmt=0,gi,gj,gx,gy,pos,i,mp,dp,own,last_own=search_own,page_count=0,last_page=0,last_date=0,j,w,h,st=0,lv;
    int is_p1=0, exp_res=GRID_X*GRID_Y, tp, view_own=0;
    int thumb_drawn[GRID_X*GRID_Y];
    pixel *v_buf = (pixel *)malloc(((YRES+MENUSIZE)*(XRES+BARSIZE))*PIXELSIZE);
    float ry;
    time_t http_last_use=HTTP_TIMEOUT;
    ui_edit ed;


    void *http = NULL;
    int active = 0;
    char *last = NULL;
    int search = 0;
    int lasttime = TIMEOUT;
    char *uri;
    int status;
    char *results;
    char *tmp, ts[64];

    void *img_http[IMGCONNS];
    char *img_id[IMGCONNS];
    void *thumb, *data;
    int thlen, dlen;

    memset(v_buf, 0, ((YRES+MENUSIZE)*(XRES+BARSIZE))*PIXELSIZE);

    memset(img_http, 0, sizeof(img_http));
    memset(img_id, 0, sizeof(img_id));

    memset(search_ids, 0, sizeof(search_ids));
    memset(search_names, 0, sizeof(search_names));
    memset(search_scoreup, 0, sizeof(search_scoreup));
    memset(search_scoredown, 0, sizeof(search_scoredown));
    memset(search_publish, 0, sizeof(search_publish));
    memset(search_owners, 0, sizeof(search_owners));
    memset(search_thumbs, 0, sizeof(search_thumbs));
    memset(search_thsizes, 0, sizeof(search_thsizes));

    memset(thumb_drawn, 0, sizeof(thumb_drawn));

    do_open = 0;

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    ed.x = 65;
    ed.y = 13;
    ed.w = XRES-200;
    ed.nx = 1;
    ed.def = "[search terms]";
    ed.focus = 1;
    ed.hide = 0;
    ed.cursor = strlen(search_expr);
    strcpy(ed.str, search_expr);

    sdl_wheel = 0;

    while(!sdl_poll())
    {
        uih = 0;
        bq = b;
        mxq = mx;
        myq = my;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        if(mx!=mxq || my!=myq || sdl_wheel || b)
            mmt = 0;
        else if(mmt<TIMEOUT)
            mmt++;

        clearrect(vid_buf, -1, -1, (XRES+BARSIZE)+1, YRES+MENUSIZE+1);

        memcpy(vid_buf, v_buf, ((YRES+MENUSIZE)*(XRES+BARSIZE))*PIXELSIZE);

        drawtext(vid_buf, 11, 13, "Search:", 192, 192, 192, 255);
        if(!last || (!active && strcmp(last, ed.str)))
            drawtext(vid_buf, 51, 11, "\x8E", 192, 160, 32, 255);
        else
            drawtext(vid_buf, 51, 11, "\x8E", 32, 64, 160, 255);
        drawtext(vid_buf, 51, 11, "\x8F", 255, 255, 255, 255);
        drawrect(vid_buf, 48, 8, XRES-182, 16, 192, 192, 192, 255);

        if(!svf_login)
        {
            search_own = 0;
            drawrect(vid_buf, XRES-64, 8, 56, 16, 96, 96, 96, 255);
            drawtext(vid_buf, XRES-61, 11, "\x94", 96, 80, 16, 255);
            drawtext(vid_buf, XRES-61, 11, "\x93", 128, 128, 128, 255);
            drawtext(vid_buf, XRES-46, 13, "My Own", 128, 128, 128, 255);
        }
        else if(search_own)
        {
            fillrect(vid_buf, XRES-65, 7, 58, 18, 255, 255, 255, 255);
            drawtext(vid_buf, XRES-61, 11, "\x94", 192, 160, 64, 255);
            drawtext(vid_buf, XRES-61, 11, "\x93", 32, 32, 32, 255);
            drawtext(vid_buf, XRES-46, 13, "My Own", 0, 0, 0, 255);
        }
        else
        {
            drawrect(vid_buf, XRES-64, 8, 56, 16, 192, 192, 192, 255);
            drawtext(vid_buf, XRES-61, 11, "\x94", 192, 160, 32, 255);
            drawtext(vid_buf, XRES-61, 11, "\x93", 255, 255, 255, 255);
            drawtext(vid_buf, XRES-46, 13, "My Own", 255, 255, 255, 255);
        }

        if(search_date)
        {
            fillrect(vid_buf, XRES-130, 7, 62, 18, 255, 255, 255, 255);
            drawtext(vid_buf, XRES-126, 11, "\xA6", 32, 32, 32, 255);
            drawtext(vid_buf, XRES-111, 13, "By date", 0, 0, 0, 255);
        }
        else
        {
            drawrect(vid_buf, XRES-129, 8, 60, 16, 192, 192, 192, 255);
            drawtext(vid_buf, XRES-126, 11, "\xA9", 144, 48, 32, 255);
            drawtext(vid_buf, XRES-126, 11, "\xA8", 32, 144, 32, 255);
            drawtext(vid_buf, XRES-126, 11, "\xA7", 255, 255, 255, 255);
            drawtext(vid_buf, XRES-111, 13, "By votes", 255, 255, 255, 255);
        }

        if(search_page)
        {
            drawtext(vid_buf, 4, YRES+MENUSIZE-16, "\x96", 255, 255, 255, 255);
            drawrect(vid_buf, 1, YRES+MENUSIZE-20, 16, 16, 255, 255, 255, 255);
        }
        if(page_count > 9)
        {
            drawtext(vid_buf, XRES-15, YRES+MENUSIZE-16, "\x95", 255, 255, 255, 255);
            drawrect(vid_buf, XRES-18, YRES+MENUSIZE-20, 16, 16, 255, 255, 255, 255);
        }

        ui_edit_draw(vid_buf, &ed);

        if((b && !bq && mx>=1 && mx<=17 && my>=YRES+MENUSIZE-20 && my<YRES+MENUSIZE-4) || sdl_wheel>0)
        {
            if(search_page)
            {
                search_page --;
                lasttime = TIMEOUT;
            }
            sdl_wheel = 0;
            uih = 1;
        }
        if((b && !bq && mx>=XRES-18 && mx<=XRES-1 && my>=YRES+MENUSIZE-20 && my<YRES+MENUSIZE-4) || sdl_wheel<0)
        {
            if(page_count>exp_res)
            {
                lasttime = TIMEOUT;
                search_page ++;
                page_count = exp_res;
            }
            sdl_wheel = 0;
            uih = 1;
        }

        tp = -1;
        if(is_p1)
        {
            drawtext(vid_buf, (XRES-textwidth("Popular tags:"))/2, 31, "Popular tags:", 255, 192, 64, 255);
            for(gj=0; gj<((GRID_Y-GRID_P)*YRES)/(GRID_Y*14); gj++)
                for(gi=0; gi<GRID_X; gi++)
                {
                    pos = gi+GRID_X*gj;
                    if(pos>TAG_MAX || !tag_names[pos])
                        break;
                    if(tag_votes[0])
                        i = 127+(128*tag_votes[pos])/tag_votes[0];
                    else
                        i = 192;
                    w = textwidth(tag_names[pos]);
                    if(w>XRES/GRID_X-5)
                        w = XRES/GRID_X-5;
                    gx = (XRES/GRID_X)*gi;
                    gy = gj*14 + 46;
                    if(mx>=gx && mx<gx+(XRES/GRID_X) && my>=gy && my<gy+14)
                    {
                        j = (i*5)/6;
                        tp = pos;
                    }
                    else
                        j = i;
                    drawtextmax(vid_buf, gx+(XRES/GRID_X-w)/2, gy, XRES/GRID_X-5, tag_names[pos], j, j, i, 255);
                }
        }

        mp = dp = -1;
        st = 0;
        for(gj=0; gj<GRID_Y; gj++)
            for(gi=0; gi<GRID_X; gi++)
            {
                if(is_p1)
                {
                    pos = gi+GRID_X*(gj-GRID_Y+GRID_P);
                    if(pos<0)
                        break;
                }
                else
                    pos = gi+GRID_X*gj;
                if(!search_ids[pos])
                    break;
                gx = ((XRES/GRID_X)*gi) + (XRES/GRID_X-XRES/GRID_S)/2;
                gy = ((((YRES-(MENUSIZE-20))+15)/GRID_Y)*gj) + ((YRES-(MENUSIZE-20))/GRID_Y-(YRES-(MENUSIZE-20))/GRID_S+10)/2 + 18;
                if(textwidth(search_names[pos]) > XRES/GRID_X-10)
                {
                    tmp = malloc(strlen(search_names[pos])+4);
                    strcpy(tmp, search_names[pos]);
                    j = textwidthx(tmp, XRES/GRID_X-15);
                    strcpy(tmp+j, "...");
                    drawtext(vid_buf, gx+XRES/(GRID_S*2)-textwidth(tmp)/2, gy+YRES/GRID_S+7, tmp, 192, 192, 192, 255);
                    free(tmp);
                }
                else
                    drawtext(vid_buf, gx+XRES/(GRID_S*2)-textwidth(search_names[pos])/2, gy+YRES/GRID_S+7, search_names[pos], 192, 192, 192, 255);
                j = textwidth(search_owners[pos]);
                if(mx>=gx+XRES/(GRID_S*2)-j/2 && mx<=gx+XRES/(GRID_S*2)+j/2 &&
                        my>=gy+YRES/GRID_S+18 && my<=gy+YRES/GRID_S+31)
                {
                    st = 1;
                    drawtext(vid_buf, gx+XRES/(GRID_S*2)-j/2, gy+YRES/GRID_S+20, search_owners[pos], 128, 128, 160, 255);
                }
                else
                    drawtext(vid_buf, gx+XRES/(GRID_S*2)-j/2, gy+YRES/GRID_S+20, search_owners[pos], 128, 128, 128, 255);
                if(search_thumbs[pos]&&thumb_drawn[pos]==0)
                {
                    render_thumb(search_thumbs[pos], search_thsizes[pos], 1, v_buf, gx, gy, GRID_S);
                    thumb_drawn[pos] = 1;
                }
                own = svf_login && (!strcmp(svf_user, search_owners[pos]) || svf_admin || svf_mod);
                if(mx>=gx-2 && mx<=gx+XRES/GRID_S+3 && my>=gy-2 && my<=gy+YRES/GRID_S+30)
                    mp = pos;
                if(own)
                {
                    if(mx>=gx+XRES/GRID_S-4 && mx<=gx+XRES/GRID_S+6 && my>=gy-6 && my<=gy+4)
                    {
                        mp = -1;
                        dp = pos;
                    }
                }
                if(mp==pos && !st)
                    drawrect(vid_buf, gx-2, gy-2, XRES/GRID_S+3, YRES/GRID_S+3, 160, 160, 192, 255);
                else
                    drawrect(vid_buf, gx-2, gy-2, XRES/GRID_S+3, YRES/GRID_S+3, 128, 128, 128, 255);
                if(own)
                {
                    if(dp == pos)
                        drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x86", 255, 48, 32, 255);
                    else
                        drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x86", 160, 48, 32, 255);
                    drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x85", 255, 255, 255, 255);
                }
                if(!search_publish[pos])
                {
                    drawtext(vid_buf, gx-6, gy-6, "\xCD", 255, 255, 255, 255);
                    drawtext(vid_buf, gx-6, gy-6, "\xCE", 212, 151, 81, 255);
                }
                if(view_own || svf_admin || svf_mod)
                {
                    sprintf(ts+1, "%d", search_votes[pos]);
                    ts[0] = 0xBB;
                    for(j=1; ts[j]; j++)
                        ts[j] = 0xBC;
                    ts[j-1] = 0xB9;
                    ts[j] = 0xBA;
                    ts[j+1] = 0;
                    w = gx+XRES/GRID_S-2-textwidth(ts);
                    h = gy+YRES/GRID_S-11;
                    drawtext(vid_buf, w, h, ts, 16, 72, 16, 255);
                    for(j=0; ts[j]; j++)
                        ts[j] -= 14;
                    drawtext(vid_buf, w, h, ts, 192, 192, 192, 255);
                    sprintf(ts, "%d", search_votes[pos]);
                    for(j=0; ts[j]; j++)
                        ts[j] += 127;
                    drawtext(vid_buf, w+3, h, ts, 255, 255, 255, 255);
                }
                if(search_scoreup[pos]>0||search_scoredown[pos]>0)
                {
                    lv = (search_scoreup[pos]>search_scoredown[pos]?search_scoreup[pos]:search_scoredown[pos]);

                    if(((YRES/GRID_S+3)/2)>lv)
                    {
                        ry = ((float)((YRES/GRID_S+3)/2)/(float)lv);
                        if(lv<8)
                        {
                            ry =  ry/(8-lv);
                        }
                        nyu = search_scoreup[pos]*ry;
                        nyd = search_scoredown[pos]*ry;
                    }
                    else
                    {
                        ry = ((float)lv/(float)((YRES/GRID_S+3)/2));
                        nyu = search_scoreup[pos]/ry;
                        nyd = search_scoredown[pos]/ry;
                    }

                    fillrect(vid_buf, gx-2+(XRES/GRID_S)+5, gy-2+((YRES/GRID_S+3)/2)-nyu, 4, nyu, 0, 187, 40, 255);
                    fillrect(vid_buf, gx-2+(XRES/GRID_S)+5, gy-2+((YRES/GRID_S+3)/2)+1, 4, nyd, 187, 40, 0, 255);

                    drawrect(vid_buf, gx-2+(XRES/GRID_S)+5, gy-2+((YRES/GRID_S+3)/2)-nyu, 4, nyu, 0, 107, 10, 255);
                    drawrect(vid_buf, gx-2+(XRES/GRID_S)+5, gy-2+((YRES/GRID_S+3)/2)+1, 4, nyd, 107, 10, 0, 255);
                }
            }

        if(mp!=-1 && mmt>=TIMEOUT/5 && !st)
        {
            gi = mp % GRID_X;
            gj = mp / GRID_X;
            if(is_p1)
                gj += GRID_Y-GRID_P;
            gx = ((XRES/GRID_X)*gi) + (XRES/GRID_X-XRES/GRID_S)/2;
            gy = (((YRES+15)/GRID_Y)*gj) + (YRES/GRID_Y-YRES/GRID_S+10)/2 + 18;
            i = w = textwidth(search_names[mp]);
            h = YRES/GRID_Z+30;
            if(w<XRES/GRID_Z) w=XRES/GRID_Z;
            gx += XRES/(GRID_S*2)-w/2;
            gy += YRES/(GRID_S*2)-h/2;
            if(gx<2) gx=2;
            if(gx+w>=XRES-2) gx=XRES-3-w;
            if(gy<32) gy=32;
            if(gy+h>=YRES+(MENUSIZE-2)) gy=YRES+(MENUSIZE-3)-h;
            clearrect(vid_buf, gx-2, gy-3, w+4, h);
            drawrect(vid_buf, gx-2, gy-3, w+4, h, 160, 160, 192, 255);
            if(search_thumbs[mp])
                render_thumb(search_thumbs[mp], search_thsizes[mp], 1, vid_buf, gx+(w-(XRES/GRID_Z))/2, gy, GRID_Z);
            drawtext(vid_buf, gx+(w-i)/2, gy+YRES/GRID_Z+4, search_names[mp], 192, 192, 192, 255);
            drawtext(vid_buf, gx+(w-textwidth(search_owners[mp]))/2, gy+YRES/GRID_Z+16, search_owners[mp], 128, 128, 128, 255);
        }

        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        ui_edit_process(mx, my, b, &ed);

        if(sdl_key==SDLK_RETURN)
        {
            if(!last || (!active && (strcmp(last, ed.str) || last_own!=search_own || last_date!=search_date || last_page!=search_page)))
                lasttime = TIMEOUT;
            else if(search_ids[0] && !search_ids[1])
            {
                bq = 0;
                b = 1;
                mp = 0;
            }
        }
        if(sdl_key==SDLK_ESCAPE)
            goto finish;

        if(b && !bq && mx>=XRES-64 && mx<=XRES-8 && my>=8 && my<=24 && svf_login)
        {
            search_own = !search_own;
            lasttime = TIMEOUT;
        }
        if(b && !bq && mx>=XRES-129 && mx<=XRES-65 && my>=8 && my<=24)
        {
            search_date = !search_date;
            lasttime = TIMEOUT;
        }

        if(b && !bq && dp!=-1)
            if(confirm_ui(vid_buf, "Do you want to delete?", search_names[dp], "Delete"))
            {
                execute_delete(vid_buf, search_ids[dp]);
                lasttime = TIMEOUT;
                if(last)
                {
                    free(last);
                    last = NULL;
                }
            }

        if(b && !bq && tp!=-1)
        {
            strncpy(ed.str, tag_names[tp], 255);
            lasttime = TIMEOUT;
        }

        if(b && !bq && mp!=-1 && st)
        {
            sprintf(ed.str, "user:%s", search_owners[mp]);
            lasttime = TIMEOUT;
        }

        if(do_open==1)
        {
            mp = 0;
        }

        if((b && !bq && mp!=-1 && !st && !uih) || do_open==1)
        {
            fillrect(vid_buf, 0, 0, XRES+BARSIZE, YRES+MENUSIZE, 0, 0, 0, 255);
            info_box(vid_buf, "Loading...");

            uri = malloc(strlen(search_ids[mp])*3+strlen(SERVER)+64);
            strcpy(uri, "http://" SERVER "/Get.api?Op=save&ID=");
            strcaturl(uri, search_ids[mp]);
            data = http_simple_get(uri, &status, &dlen);
            free(uri);

            if(status == 200)
            {
                status = parse_save(data, dlen, 1, 0, 0);
                switch(status)
                {
                case 1:
                    error_ui(vid_buf, 0, "Simulation corrupted");
                    break;
                case 2:
                    error_ui(vid_buf, 0, "Simulation from a newer version");
                    break;
                case 3:
                    error_ui(vid_buf, 0, "Simulation on a too large grid");
                    break;
                }
                if(!status)
                {
                    char *tnames[] = {"ID", NULL};
                    char *tparts[1];
                    int tplens[1];
                    if(svf_last)
                        free(svf_last);
                    svf_last = data;
                    svf_lsize = dlen;

                    tparts[0] = search_ids[mp];
                    tplens[0] = strlen(search_ids[mp]);
                    data = http_multipart_post("http://" SERVER "/Tags.api", tnames, tparts, tplens, svf_user, svf_pass, &status, NULL);

                    svf_open = 1;
                    svf_own = svf_login && !strcmp(search_owners[mp], svf_user);
                    svf_publish = search_publish[mp] && svf_login && !strcmp(search_owners[mp], svf_user);

                    strcpy(svf_id, search_ids[mp]);
                    strcpy(svf_name, search_names[mp]);
                    if(status == 200)
                    {
                        if(data)
                        {
                            strncpy(svf_tags, data, 255);
                            svf_tags[255] = 0;
                        }
                        else
                            svf_tags[0] = 0;
                    }
                    else
                    {
                        svf_tags[0] = 0;
                    }

                    if(svf_login)
                    {
                        char *names[] = {"ID", NULL};
                        char *parts[1];
                        parts[0] = search_ids[mp];
                        data = http_multipart_post("http://" SERVER "/Vote.api", names, parts, NULL, svf_user, svf_pass, &status, NULL);
                        if(status == 200)
                        {
                            if(data)
                            {
                                if(!strcmp(data, "Up"))
                                {
                                    svf_myvote = 1;
                                }
                                else if(!strcmp(data, "Down"))
                                {
                                    svf_myvote = -1;
                                }
                                else
                                {
                                    svf_myvote = 0;
                                }
                            }
                            else
                            {
                                svf_myvote = 0;
                            }
                        }
                        else
                        {
                            svf_myvote = 0;
                        }
                    }
                }
                else
                {
                    svf_open = 0;
                    svf_publish = 0;
                    svf_own = 0;
                    svf_myvote = 0;
                    svf_id[0] = 0;
                    svf_name[0] = 0;
                    svf_tags[0] = 0;
                    if(svf_last)
                        free(svf_last);
                    svf_last = NULL;
                }
            }
            else
                error_ui(vid_buf, status, http_ret_text(status));

            if(data)
                free(data);
            goto finish;
        }

        if(!last)
        {
            search = 1;
        }
        else if(!active && (strcmp(last, ed.str) || last_own!=search_own || last_date!=search_date || last_page!=search_page))
        {
            search = 1;
            if(strcmp(last, ed.str) || last_own!=search_own || last_date!=search_date)
            {
                search_page = 0;
                page_count = 0;
            }
            free(last);
            last = NULL;
        }
        else
            search = 0;

        if(search && lasttime>=TIMEOUT)
        {
            lasttime = 0;
            last = mystrdup(ed.str);
            last_own = search_own;
            last_date = search_date;
            last_page = search_page;
            active = 1;
            uri = malloc(strlen(last)*3+80+strlen(SERVER)+strlen(svf_user));
            if(search_own || svf_admin || svf_mod)
                tmp = "&ShowVotes=true";
            else
                tmp = "";
            if(!search_own && !search_date && !*last)
            {
                if(search_page)
                {
                    exp_res = GRID_X*GRID_Y;
                    sprintf(uri, "http://" SERVER "/Search.api?Start=%d&Count=%d%s&Query=", (search_page-1)*GRID_X*GRID_Y+GRID_X*GRID_P, exp_res+1, tmp);
                }
                else
                {
                    exp_res = GRID_X*GRID_P;
                    sprintf(uri, "http://" SERVER "/Search.api?Start=%d&Count=%d&t=%d%s&Query=", 0, exp_res+1, ((GRID_Y-GRID_P)*YRES)/(GRID_Y*14)*GRID_X, tmp);
                }
            }
            else
            {
                exp_res = GRID_X*GRID_Y;
                sprintf(uri, "http://" SERVER "/Search.api?Start=%d&Count=%d%s&Query=", search_page*GRID_X*GRID_Y, exp_res+1, tmp);
            }
            strcaturl(uri, last);
            if(search_own)
            {
                strcaturl(uri, " user:");
                strcaturl(uri, svf_user);
            }
            if(search_date)
                strcaturl(uri, " sort:date");

            http = http_async_req_start(http, uri, NULL, 0, 1);
            if(svf_login)
            {
                http_auth_headers(http, svf_user, svf_pass);
            }
            http_last_use = time(NULL);
            free(uri);
        }

        if(active && http_async_req_status(http))
        {
            http_last_use = time(NULL);
            results = http_async_req_stop(http, &status, NULL);
            view_own = last_own;
            if(status == 200)
            {
                page_count = search_results(results, last_own||svf_admin||svf_mod);
                memset(thumb_drawn, 0, sizeof(thumb_drawn));
                memset(v_buf, 0, ((YRES+MENUSIZE)*(XRES+BARSIZE))*PIXELSIZE);
            }
            is_p1 = (exp_res < GRID_X*GRID_Y);
            free(results);
            active = 0;
        }

        if(http && !active && (time(NULL)>http_last_use+HTTP_TIMEOUT))
        {
            http_async_req_close(http);
            http = NULL;
        }

        for(i=0; i<IMGCONNS; i++)
        {
            if(img_http[i] && http_async_req_status(img_http[i]))
            {
                thumb = http_async_req_stop(img_http[i], &status, &thlen);
                if(status != 200)
                {
                    if(thumb)
                        free(thumb);
                    thumb = calloc(1,4);
                    thlen = 4;
                }
                thumb_cache_add(img_id[i], thumb, thlen);
                for(pos=0; pos<GRID_X*GRID_Y; pos++)
                    if(search_ids[pos] && !strcmp(search_ids[pos], img_id[i]))
                        break;
                if(pos<GRID_X*GRID_Y)
                {
                    search_thumbs[pos] = thumb;
                    search_thsizes[pos] = thlen;
                }
                else
                    free(thumb);
                free(img_id[i]);
                img_id[i] = NULL;
            }
            if(!img_id[i])
            {
                for(pos=0; pos<GRID_X*GRID_Y; pos++)
                    if(search_ids[pos] && !search_thumbs[pos])
                    {
                        for(gi=0; gi<IMGCONNS; gi++)
                            if(img_id[gi] && !strcmp(search_ids[pos], img_id[gi]))
                                break;
                        if(gi<IMGCONNS)
                            continue;
                        break;
                    }
                if(pos<GRID_X*GRID_Y)
                {
                    uri = malloc(strlen(search_ids[pos])*3+strlen(SERVER)+64);
                    strcpy(uri, "http://" SERVER "/Get.api?Op=thumb&ID=");
                    strcaturl(uri, search_ids[pos]);
                    img_id[i] = mystrdup(search_ids[pos]);
                    img_http[i] = http_async_req_start(img_http[i], uri, NULL, 0, 1);
                    free(uri);
                }
            }
            if(!img_id[i] && img_http[i])
            {
                http_async_req_close(img_http[i]);
                img_http[i] = NULL;
            }
        }

        if(lasttime<TIMEOUT)
            lasttime++;
    }

finish:
    if(last)
        free(last);
    if(http)
        http_async_req_close(http);
    for(i=0; i<IMGCONNS; i++)
        if(img_http[i])
            http_async_req_close(img_http[i]);

    search_results("", 0);

    strcpy(search_expr, ed.str);

    return 0;
}

void draw_image(pixel *vid, pixel *img, int x, int y, int w, int h, int a)
{
    int i, j, r, g, b;
    for(j=0; j<h; j++)
        for(i=0; i<w; i++)
        {
            r = PIXR(*img);
            g = PIXG(*img);
            b = PIXB(*img);
            drawpixel(vid, x+i, y+j, r, g, b, a);
            img++;
        }
}

int stamp_ui(pixel *vid_buf)
{
    int b=1,bq,mx,my,d=-1,i,j,k,x,gx,gy,y,w,h,r=-1,stamp_page=0,per_page=STAMP_X*STAMP_Y,page_count;
    char page_info[64];
    page_count = ceil((float)stamp_count/(float)per_page);

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        clearrect(vid_buf, -1, -1, XRES+1, YRES+MENUSIZE+1);
        k = stamp_page*per_page;//0;
        r = -1;
        d = -1;
        for(j=0; j<GRID_Y; j++)
            for(i=0; i<GRID_X; i++)
            {
                if(stamps[k].name[0] && stamps[k].thumb)
                {
                    gx = ((XRES/GRID_X)*i) + (XRES/GRID_X-XRES/GRID_S)/2;
                    gy = ((((YRES-MENUSIZE+20)+15)/GRID_Y)*j) + ((YRES-MENUSIZE+20)/GRID_Y-(YRES-MENUSIZE+20)/GRID_S+10)/2 + 18;
                    x = (XRES*i)/GRID_X + XRES/(GRID_X*2);
                    y = (YRES*j)/GRID_Y + YRES/(GRID_Y*2);
                    gy -= 20;
                    w = stamps[k].thumb_w;
                    h = stamps[k].thumb_h;
                    x -= w/2;
                    y -= h/2;
                    draw_image(vid_buf, stamps[k].thumb, gx+(((XRES/GRID_S)/2)-(w/2)), gy+(((YRES/GRID_S)/2)-(h/2)), w, h, 255);
                    xor_rect(vid_buf, gx+(((XRES/GRID_S)/2)-(w/2)), gy+(((YRES/GRID_S)/2)-(h/2)), w, h);
                    if(mx>=gx+XRES/GRID_S-4 && mx<(gx+XRES/GRID_S)+6 && my>=gy-6 && my<gy+4)
                    {
                        d = k;
                        drawrect(vid_buf, gx-2, gy-2, XRES/GRID_S+3, YRES/GRID_S+3, 128, 128, 128, 255);
                        drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x86", 255, 48, 32, 255);
                    }
                    else
                    {
                        if(mx>=gx && mx<gx+(XRES/GRID_S) && my>=gy && my<gy+(YRES/GRID_S))
                        {
                            r = k;
                            drawrect(vid_buf, gx-2, gy-2, XRES/GRID_S+3, YRES/GRID_S+3, 128, 128, 210, 255);
                        }
                        else
                        {
                            drawrect(vid_buf, gx-2, gy-2, XRES/GRID_S+3, YRES/GRID_S+3, 128, 128, 128, 255);
                        }
                        drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x86", 150, 48, 32, 255);
                    }
                    drawtext(vid_buf, gx+XRES/(GRID_S*2)-textwidth(stamps[k].name)/2, gy+YRES/GRID_S+7, stamps[k].name, 192, 192, 192, 255);
                    drawtext(vid_buf, gx+XRES/GRID_S-4, gy-6, "\x85", 255, 255, 255, 255);
                }
                k++;
            }

        sprintf(page_info, "Page %d of %d", stamp_page+1, page_count);

        drawtext(vid_buf, (XRES/2)-(textwidth(page_info)/2), YRES+MENUSIZE-14, page_info, 255, 255, 255, 255);

        if(stamp_page)
        {
            drawtext(vid_buf, 4, YRES+MENUSIZE-14, "\x96", 255, 255, 255, 255);
            drawrect(vid_buf, 1, YRES+MENUSIZE-18, 16, 16, 255, 255, 255, 255);
        }
        if(stamp_page<page_count-1)
        {
            drawtext(vid_buf, XRES-15, YRES+MENUSIZE-14, "\x95", 255, 255, 255, 255);
            drawrect(vid_buf, XRES-18, YRES+MENUSIZE-18, 16, 16, 255, 255, 255, 255);
        }

        if(b==1&&d!=-1)
        {
            if(confirm_ui(vid_buf, "Do you want to delete?", stamps[d].name, "Delete"))
            {
                del_stamp(d);
            }
        }

        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        if(b==1&&r!=-1)
            break;
        if(b==4&&r!=-1)
        {
            r = -1;
            break;
        }

        if((b && !bq && mx>=1 && mx<=17 && my>=YRES+MENUSIZE-18 && my<YRES+MENUSIZE-2) || sdl_wheel>0)
        {
            if(stamp_page)
            {
                stamp_page --;
            }
            sdl_wheel = 0;
        }
        if((b && !bq && mx>=XRES-18 && mx<=XRES-1 && my>=YRES+MENUSIZE-18 && my<YRES+MENUSIZE-2) || sdl_wheel<0)
        {
            if(stamp_page<page_count-1)
            {
                stamp_page ++;
            }
            sdl_wheel = 0;
        }

        if(sdl_key==SDLK_RETURN)
            break;
        if(sdl_key==SDLK_ESCAPE)
        {
            r = -1;
            break;
        }
    }

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    return r;
}

/***********************************************************
 *                      MESSAGE SIGNS                      *
 ***********************************************************/

void get_sign_pos(int i, int *x0, int *y0, int *w, int *h)
{
    //Changing width if sign have special content
    if(strcmp(signs[i].text, "{p}")==0)
        *w = textwidth("Pressure: -000.00");

    if(strcmp(signs[i].text, "{t}")==0)
        *w = textwidth("Temp: 0000.00");

    //Ususal width
    if(strcmp(signs[i].text, "{p}") && strcmp(signs[i].text, "{t}"))
        *w = textwidth(signs[i].text) + 5;
    *h = 14;
    *x0 = (signs[i].ju == 2) ? signs[i].x - *w :
          (signs[i].ju == 1) ? signs[i].x - *w/2 : signs[i].x;
    *y0 = (signs[i].y > 18) ? signs[i].y - 18 : signs[i].y + 4;
}

void render_signs(pixel *vid_buf)
{
    int i, j, x, y, w, h, dx, dy;
    char buff[30];  //Buffer
    for(i=0; i<MAXSIGNS; i++)
        if(signs[i].text[0])
        {
            get_sign_pos(i, &x, &y, &w, &h);
            clearrect(vid_buf, x, y, w, h);
            drawrect(vid_buf, x, y, w, h, 192, 192, 192, 255);

            //Displaying special information
            if(strcmp(signs[i].text, "{p}")==0)
            {
                sprintf(buff, "Pressure: %3.2f", pv[signs[i].y/CELL][signs[i].x/CELL]);  //...pressure
                drawtext(vid_buf, x+3, y+3, buff, 255, 255, 255, 255);
            }

            if(strcmp(signs[i].text, "{t}")==0)
            {
                if((pmap[signs[i].y][signs[i].x]>>8)>0 && (pmap[signs[i].y][signs[i].x]>>8)<NPART)
                    sprintf(buff, "Temp: %4.2f", parts[pmap[signs[i].y][signs[i].x]>>8].temp);  //...tempirature
                else
                    sprintf(buff, "Temp: 0.00");  //...tempirature
                drawtext(vid_buf, x+3, y+3, buff, 255, 255, 255, 255);
            }

            //Usual text
            if(strcmp(signs[i].text, "{p}") && strcmp(signs[i].text, "{t}"))
                drawtext(vid_buf, x+3, y+3, signs[i].text, 255, 255, 255, 255);
            x = signs[i].x;
            y = signs[i].y;
            dx = 1 - signs[i].ju;
            dy = (signs[i].y > 18) ? -1 : 1;
            for(j=0; j<4; j++)
            {
                drawpixel(vid_buf, x, y, 192, 192, 192, 255);
                x+=dx;
                y+=dy;
            }
        }
}

void add_sign_ui(pixel *vid_buf, int mx, int my)
{
    int i, w, h, x, y, nm=0, ju;
    int x0=(XRES-192)/2,y0=(YRES-80)/2,b=1,bq;
    ui_edit ed;

    // check if it is an existing sign
    for(i=0; i<MAXSIGNS; i++)
        if(signs[i].text[0])
        {
            get_sign_pos(i, &x, &y, &w, &h);
            if(mx>=x && mx<=x+w && my>=y && my<=y+h)
                break;
        }
    // else look for empty spot
    if(i >= MAXSIGNS)
    {
        nm = 1;
        for(i=0; i<MAXSIGNS; i++)
            if(!signs[i].text[0])
                break;
    }
    if(i >= MAXSIGNS)
        return;

    if(nm)
    {
        signs[i].x = mx;
        signs[i].y = my;
        signs[i].ju = 1;
    }

    while(!sdl_poll())
    {
        b = SDL_GetMouseState(&mx, &my);
        if(!b)
            break;
    }

    ed.x = x0+25;
    ed.y = y0+25;
    ed.w = 158;
    ed.nx = 1;
    ed.def = "[message]";
    ed.focus = 1;
    ed.hide = 0;
    ed.cursor = strlen(signs[i].text);
    strcpy(ed.str, signs[i].text);
    ju = signs[i].ju;

    fillrect(vid_buf, -1, -1, XRES, YRES+MENUSIZE, 0, 0, 0, 192);
    while(!sdl_poll())
    {
        bq = b;
        b = SDL_GetMouseState(&mx, &my);
        mx /= sdl_scale;
        my /= sdl_scale;

        drawrect(vid_buf, x0, y0, 192, 80, 192, 192, 192, 255);
        clearrect(vid_buf, x0, y0, 192, 80);
        drawtext(vid_buf, x0+8, y0+8, nm ? "New sign:" : "Edit sign:", 255, 255, 255, 255);
        drawtext(vid_buf, x0+12, y0+23, "\xA1", 32, 64, 128, 255);
        drawtext(vid_buf, x0+12, y0+23, "\xA0", 255, 255, 255, 255);
        drawrect(vid_buf, x0+8, y0+20, 176, 16, 192, 192, 192, 255);
        ui_edit_draw(vid_buf, &ed);
        drawtext(vid_buf, x0+8, y0+46, "Justify:", 255, 255, 255, 255);
        draw_icon(vid_buf, x0+50, y0+42, 0x9D, ju == 0);
        draw_icon(vid_buf, x0+68, y0+42, 0x9E, ju == 1);
        draw_icon(vid_buf, x0+86, y0+42, 0x9F, ju == 2);

        if(!nm)
        {
            drawtext(vid_buf, x0+138, y0+45, "\x86", 160, 48, 32, 255);
            drawtext(vid_buf, x0+138, y0+45, "\x85", 255, 255, 255, 255);
            drawtext(vid_buf, x0+152, y0+46, "Delete", 255, 255, 255, 255);
            drawrect(vid_buf, x0+134, y0+42, 50, 15, 255, 255, 255, 255);
        }

        drawtext(vid_buf, x0+5, y0+69, "OK", 255, 255, 255, 255);
        drawrect(vid_buf, x0, y0+64, 192, 16, 192, 192, 192, 255);

        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));

        ui_edit_process(mx, my, b, &ed);

        if(b && !bq && mx>=x0+50 && mx<=x0+67 && my>=y0+42 && my<=y0+59)
            ju = 0;
        if(b && !bq && mx>=x0+68 && mx<=x0+85 && my>=y0+42 && my<=y0+59)
            ju = 1;
        if(b && !bq && mx>=x0+86 && mx<=x0+103 && my>=y0+42 && my<=y0+59)
            ju = 2;

        if(b && !bq && mx>=x0+9 && mx<x0+23 && my>=y0+22 && my<y0+36)
            break;
        if(b && !bq && mx>=x0 && mx<x0+192 && my>=y0+64 && my<=y0+80)
            break;

        if(!nm && b && !bq && mx>=x0+134 && my>=y0+42 && mx<=x0+184 && my<=y0+59)
        {
            signs[i].text[0] = 0;
            return;
        }

        if(sdl_key==SDLK_RETURN)
            break;
        if(sdl_key==SDLK_ESCAPE)
        {
            if(!ed.focus)
                return;
            ed.focus = 0;
        }
    }

    strcpy(signs[i].text, ed.str);
    signs[i].ju = ju;
}

/***********************************************************
 *                       CONFIG FILE                       *
 ***********************************************************/

char http_proxy[256] = "";

void save_string(FILE *f, char *str)
{
    int li = strlen(str);
    unsigned char lb[2];
    lb[0] = li;
    lb[1] = li >> 8;
    fwrite(lb, 2, 1, f);
    fwrite(str, li, 1, f);
}

int load_string(FILE *f, char *str, int max)
{
    int li;
    unsigned char lb[2];
    fread(lb, 2, 1, f);
    li = lb[0] | (lb[1] << 8);
    if(li > max)
    {
        str[0] = 0;
        return 1;
    }
    fread(str, li, 1, f);
    str[li] = 0;
    return 0;
}

unsigned char last_major=0, last_minor=0, update_flag=0;

void save_presets(int do_update)
{
    FILE *f=fopen("powder.def", "wb");
    unsigned char sig[4] = {0x50, 0x44, 0x65, 0x66};
    unsigned char tmp = sdl_scale;
    if(!f)
        return;
    fwrite(sig, 1, 4, f);
    save_string(f, svf_user);
    save_string(f, svf_pass);
    fwrite(&tmp, 1, 1, f);
    tmp = cmode;
    fwrite(&tmp, 1, 1, f);
    tmp = svf_admin;
    fwrite(&tmp, 1, 1, f);
    tmp = svf_mod;
    fwrite(&tmp, 1, 1, f);
    save_string(f, http_proxy);
    tmp = SAVE_VERSION;
    fwrite(&tmp, 1, 1, f);
    tmp = MINOR_VERSION;
    fwrite(&tmp, 1, 1, f);
    tmp = do_update;
    fwrite(&tmp, 1, 1, f);
    fclose(f);
}

void load_presets(void)
{
    FILE *f=fopen("powder.def", "rb");
    unsigned char sig[4], tmp;
    if(!f)
        return;
    fread(sig, 1, 4, f);
    if(sig[0]!=0x50 || sig[1]!=0x44 || sig[2]!=0x65 || sig[3]!=0x66)
    {
        if(sig[0]==0x4D && sig[1]==0x6F && sig[2]==0x46 && sig[3]==0x6F)
        {
            if(fseek(f, -3, SEEK_END))
            {
                remove("powder.def");
                return;
            }
            if(fread(sig, 1, 3, f) != 3)
            {
                remove("powder.def");
                goto fail;
            }
            last_major = sig[0];
            last_minor = sig[1];
            update_flag = sig[2];
        }
        fclose(f);
        remove("powder.def");
        return;
    }
    if(load_string(f, svf_user, 63))
        goto fail;
    if(load_string(f, svf_pass, 63))
        goto fail;
    svf_login = !!svf_user[0];
    if(fread(&tmp, 1, 1, f) != 1)
        goto fail;
    sdl_scale = (tmp == 2) ? 2 : 1;
    if(fread(&tmp, 1, 1, f) != 1)
        goto fail;
    cmode = tmp%7;
    if(fread(&tmp, 1, 1, f) != 1)
        goto fail;
    svf_admin = tmp;
    if(fread(&tmp, 1, 1, f) != 1)
        goto fail;
    svf_mod = tmp;
    if(load_string(f, http_proxy, 255))
        goto fail;
    if(fread(sig, 1, 3, f) != 3)
        goto fail;
    last_major = sig[0];
    last_minor = sig[1];
    update_flag = sig[2];
fail:
    fclose(f);
}

void dim_copy(pixel *dst, pixel *src)
{
    int i,r,g,b;
    for(i=0; i<XRES*YRES; i++)
    {
        r = PIXR(src[i]);
        g = PIXG(src[i]);
        b = PIXB(src[i]);
        if(r>0)
            r--;
        if(g>0)
            g--;
        if(b>0)
            b--;
        dst[i] = PIXRGB(r,g,b);
    }
}

unsigned int fire_alpha[CELL*3][CELL*3];
void prepare_alpha(void)
{
    int x,y,i,j;
    float temp[CELL*3][CELL*3];
    memset(temp, 0, sizeof(temp));
    for(x=0; x<CELL; x++)
        for(y=0; y<CELL; y++)
            for(i=-CELL; i<CELL; i++)
                for(j=-CELL; j<CELL; j++)
                    temp[y+CELL+j][x+CELL+i] += expf(-0.1f*(i*i+j*j));
    for(x=0; x<CELL*3; x++)
        for(y=0; y<CELL*3; y++)
            fire_alpha[y][x] = (int)(255.0f*temp[y][x]/(CELL*CELL));
}

void render_fire(pixel *dst)
{
    int i,j,x,y,r,g,b;
    for(j=0; j<YRES/CELL; j++)
        for(i=0; i<XRES/CELL; i++)
        {
            r = fire_r[j][i];
            g = fire_g[j][i];
            b = fire_b[j][i];
            if(r || g || b)
                for(y=-CELL+1; y<2*CELL; y++)
                    for(x=-CELL+1; x<2*CELL; x++)
                        addpixel(dst, i*CELL+x, j*CELL+y, r, g, b, fire_alpha[y+CELL][x+CELL]);
            for(y=-1; y<2; y++)
                for(x=-1; x<2; x++)
                    if(i+x>=0 && j+y>=0 && i+x<XRES/CELL && j+y<YRES/CELL && (x || y))
                    {
                        r += fire_r[j+y][i+x] / 8;
                        g += fire_g[j+y][i+x] / 8;
                        b += fire_b[j+y][i+x] / 8;
                    }
            r /= 2;
            g /= 2;
            b /= 2;
            fire_r[j][i] = r>4 ? r-4 : 0;
            fire_g[j][i] = g>4 ? g-4 : 0;
            fire_b[j][i] = b>4 ? b-4 : 0;
        }
}

int zoom_en = 0;
int zoom_x=(XRES-ZSIZE_D)/2, zoom_y=(YRES-ZSIZE_D)/2;
int zoom_wx=0, zoom_wy=0;
void render_zoom(pixel *img)
{
    int x, y, i, j;
    pixel pix;
    drawrect(img, zoom_wx-2, zoom_wy-2, ZSIZE*ZFACTOR+2, ZSIZE*ZFACTOR+2, 192, 192, 192, 255);
    drawrect(img, zoom_wx-1, zoom_wy-1, ZSIZE*ZFACTOR, ZSIZE*ZFACTOR, 0, 0, 0, 255);
    clearrect(img, zoom_wx, zoom_wy, ZSIZE*ZFACTOR, ZSIZE*ZFACTOR);
    for(j=0; j<ZSIZE; j++)
        for(i=0; i<ZSIZE; i++)
        {
            pix = img[(j+zoom_y)*(XRES+BARSIZE)+(i+zoom_x)];
            for(y=0; y<ZFACTOR-1; y++)
                for(x=0; x<ZFACTOR-1; x++)
                    img[(j*ZFACTOR+y+zoom_wy)*(XRES+BARSIZE)+(i*ZFACTOR+x+zoom_wx)] = pix;
        }
    if(zoom_en)
    {
        for(j=-1; j<=ZSIZE; j++)
        {
            xor_pixel(zoom_x+j, zoom_y-1, img);
            xor_pixel(zoom_x+j, zoom_y+ZSIZE, img);
        }
        for(j=0; j<ZSIZE; j++)
        {
            xor_pixel(zoom_x-1, zoom_y+j, img);
            xor_pixel(zoom_x+ZSIZE, zoom_y+j, img);
        }
    }
}

void render_cursor(pixel *vid, int x, int y, int t, int r)
{
    int i,j,c;
    if(t<PT_NUM||t==SPC_AIR||t==SPC_HEAT||t==SPC_COOL||t==SPC_VACUUM)
    {
        if(r<=0)
            xor_pixel(x, y, vid);
        else
            for(j=0; j<=r; j++)
                for(i=0; i<=r; i++)
                    if(i*i+j*j<=r*r && ((i+1)*(i+1)+j*j>r*r || i*i+(j+1)*(j+1)>r*r))
                    {
                        xor_pixel(x+i, y+j, vid);
                        if(j) xor_pixel(x+i, y-j, vid);
                        if(i) xor_pixel(x-i, y+j, vid);
                        if(i&&j) xor_pixel(x-i, y-j, vid);
                    }
    }
    else
    {
        int tc;
        c = (r/CELL) * CELL;
        x = (x/CELL) * CELL;
        y = (y/CELL) * CELL;

        tc = !((c%(CELL*2))==0);

        x -= c/2;
        y -= c/2;

        x += tc*(CELL/2);
        y += tc*(CELL/2);

        for(i=0; i<CELL+c; i++)
        {
            xor_pixel(x+i, y, vid);
            xor_pixel(x+i, y+CELL+c-1, vid);
        }
        for(i=1; i<CELL+c-1; i++)
        {
            xor_pixel(x, y+i, vid);
            xor_pixel(x+CELL+c-1, y+i, vid);
        }
    }
}

#ifdef WIN32
#define x86_cpuid(func,af,bf,cf,df) \
	do {\
	__asm mov	eax, func\
	__asm cpuid\
	__asm mov	af, eax\
	__asm mov	bf, ebx\
	__asm mov	cf, ecx\
	__asm mov	df, edx\
	} while(0)
#else
#define x86_cpuid(func,af,bf,cf,df) \
__asm__ __volatile ("cpuid":\
	"=a" (af), "=b" (bf), "=c" (cf), "=d" (df) : "a" (func));
#endif


int cpu_check(void)
{
#ifdef MACOSX
    return 0;
#else
#ifdef X86
    unsigned af,bf,cf,df;
    x86_cpuid(0, af, bf, cf, df);
    if(bf==0x68747541 && cf==0x444D4163 && df==0x69746E65)
        amd = 1;
    x86_cpuid(1, af, bf, cf, df);
#ifdef X86_SSE
    if(!(df&(1<<25)))
        return 1;
#endif
#ifdef X86_SSE2
    if(!(df&(1<<26)))
        return 1;
#endif
#ifdef X86_SSE3
    if(!(cf&1))
        return 1;
#endif
#endif
#endif
    return 0;
}

char *tag = "(c) 2008-9 Stanislaw Skowronek";
int itc = 0;
char itc_msg[64] = "[?]";

pixel *fire_bg;
void set_cmode(int cm)
{
    cmode = cm;
    itc = 51;
    if(cmode==4)
    {
        memset(fire_r, 0, sizeof(fire_r));
        memset(fire_g, 0, sizeof(fire_g));
        memset(fire_b, 0, sizeof(fire_b));
        strcpy(itc_msg, "Blob Display");
    }
    else if(cmode==5)
    {
        strcpy(itc_msg, "Heat Display");
    }
    else if(cmode==6)
    {
        memset(fire_r, 0, sizeof(fire_r));
        memset(fire_g, 0, sizeof(fire_g));
        memset(fire_b, 0, sizeof(fire_b));
        strcpy(itc_msg, "Fancy Display");
    }
    else if(cmode==3)
    {
        memset(fire_r, 0, sizeof(fire_r));
        memset(fire_g, 0, sizeof(fire_g));
        memset(fire_b, 0, sizeof(fire_b));
        strcpy(itc_msg, "Fire Display");
    }
    else if(cmode==2)
    {
        memset(fire_bg, 0, XRES*YRES*PIXELSIZE);
        strcpy(itc_msg, "Persistent Display");
    }
    else if(cmode==1)
        strcpy(itc_msg, "Pressure Display");
    else
        strcpy(itc_msg, "Velocity Display");
}

char my_uri[] = "http://" SERVER "/Update.api?Action=Download&Architecture="
#if defined WIN32
                "Windows32"
#elif defined LIN32
                "Linux32"
#elif defined LIN64
                "Linux64"
#elif defined MACOSX
                "MacOSX"
#else
                "Unknown"
#endif
                "&InstructionSet="
#if defined X86_SSE3
                "SSE3"
#elif defined X86_SSE2
                "SSE2"
#elif defined X86_SSE
                "SSE"
#else
                "SSE"
#endif
                ;

char *download_ui(pixel *vid_buf, char *uri, int *len)
{
    int dstate = 0;
    void *http = http_async_req_start(NULL, uri, NULL, 0, 0);
    int x0=(XRES-240)/2,y0=(YRES-MENUSIZE)/2;
    int done, total, i, ret, zlen, ulen;
    char str[16], *tmp, *res;

    while(!http_async_req_status(http))
    {
        sdl_poll();

        http_async_get_length(http, &total, &done);

        clearrect(vid_buf, x0-2, y0-2, 244, 64);
        drawrect(vid_buf, x0, y0, 240, 60, 192, 192, 192, 255);
        drawtext(vid_buf, x0+8, y0+8, "Please wait", 255, 216, 32, 255);
        drawtext(vid_buf, x0+8, y0+26, "Downloading update...", 255, 255, 255, 255);

        if(total)
        {
            i = (236*done)/total;
            fillrect(vid_buf, x0+1, y0+45, i+1, 14, 255, 216, 32, 255);
            i = (100*done)/total;
            sprintf(str, "%d%%", i);
            if(i<50)
                drawtext(vid_buf, x0+120-textwidth(str)/2, y0+48, str, 192, 192, 192, 255);
            else
                drawtext(vid_buf, x0+120-textwidth(str)/2, y0+48, str, 0, 0, 0, 255);
        }
        else
            drawtext(vid_buf, x0+120-textwidth("Waiting...")/2, y0+48, "Waiting...", 255, 216, 32, 255);

        drawrect(vid_buf, x0, y0+44, 240, 16, 192, 192, 192, 255);
        sdl_blit(0, 0, (XRES+BARSIZE), YRES+MENUSIZE, vid_buf, (XRES+BARSIZE));
    }

    tmp = http_async_req_stop(http, &ret, &zlen);
    if(ret!=200)
    {
        error_ui(vid_buf, ret, http_ret_text(ret));
        if(tmp)
            free(tmp);
        return NULL;
    }
    if(!tmp)
    {
        error_ui(vid_buf, 0, "Server did not return data");
        return NULL;
    }

    if(zlen<16)
    {
        printf("ZLen is not 16!\n");
        goto corrupt;
    }
    if(tmp[0]!=0x42 || tmp[1]!=0x75 || tmp[2]!=0x54 || tmp[3]!=0x54)
    {
        printf("Tmperr %d, %d, %d, %d\n", tmp[0], tmp[1], tmp[2], tmp[3]);
        goto corrupt;
    }

    ulen  = (unsigned char)tmp[4];
    ulen |= ((unsigned char)tmp[5])<<8;
    ulen |= ((unsigned char)tmp[6])<<16;
    ulen |= ((unsigned char)tmp[7])<<24;

    res = (char *)malloc(ulen);
    if(!res)
    {
        printf("No res!\n");
        goto corrupt;
    }
    dstate = BZ2_bzBuffToBuffDecompress((char *)res, (unsigned *)&ulen, (char *)(tmp+8), zlen-8, 0, 0);
    if(dstate)
    {
        printf("Decompression failure: %d!\n", dstate);
        free(res);
        goto corrupt;
    }

    free(tmp);
    if(len)
        *len = ulen;
    return res;

corrupt:
    error_ui(vid_buf, 0, "Downloaded update is corrupted");
    free(tmp);
    return NULL;
}

void clear_area(int area_x, int area_y, int area_w, int area_h)
{
    int cx = 0;
    int cy = 0;
    for(cy=0; cy<area_h; cy++)
    {
        for(cx=0; cx<area_w; cx++)
        {
            bmap[(cy+area_y)/CELL][(cx+area_x)/CELL] = 0;
            delete_part(cx+area_x, cy+area_y);
        }
    }
}

int main(int argc, char *argv[])
{
    int hud_enable = 1;
    int active_menu = 0;
#ifdef BETA
    int is_beta = 0;
#endif
    char uitext[48] = "";
    char heattext[64] = "";
    int currentTime = 0;
    int FPS = 0, FPSB = 0;
    int pastFPS = 0;
    int past = 0;
    pixel *vid_buf=calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
    void *http_ver_check;
    char *ver_data=NULL, *tmp;
    int i, j, bq, fire_fc=0, do_check=0, old_version=0, http_ret=0, major, minor, old_ver_len;
#ifdef INTERNAL
    int vs = 0;
#endif
    int x, y, b = 0, sl=1, sr=0, su=0, c, lb = 0, lx = 0, ly = 0, lm = 0;//, tx, ty;
    int da = 0, db = 0, it = 2047, mx, my, bs = 2;
    float nfvx, nfvy;
    int load_mode=0, load_w=0, load_h=0, load_x=0, load_y=0, load_size=0;
    void *load_data=NULL;
    pixel *load_img=NULL;//, *fbi_img=NULL;
    int save_mode=0, save_x=0, save_y=0, save_w=0, save_h=0, copy_mode=0;

#ifdef MT
    numCores = core_count();
#endif

#ifdef BETA
    if(is_beta)
    {
        old_ver_len = textwidth(old_ver_msg_beta);
    }
    else
    {
        old_ver_len = textwidth(old_ver_msg);
    }
#else
    old_ver_len = textwidth(old_ver_msg);
#endif
    menu_count();
    parts = calloc(sizeof(particle), NPART);
    cb_parts = calloc(sizeof(particle), NPART);
    for(i=0; i<NPART-1; i++)
        parts[i].life = i+1;
    parts[NPART-1].life = -1;
    pfree = 0;
    fire_bg=calloc(XRES*YRES, PIXELSIZE);
    memset(signs, 0, sizeof(signs));

    //fbi_img = render_packed_rgb(fbi, FBI_W, FBI_H, FBI_CMP);

    load_presets();

    for(i=1; i<argc; i++)
    {
        if(!strncmp(argv[i], "scale:", 6))
        {
            sdl_scale = (argv[i][6]=='2') ? 2 : 1;
        }
        else if(!strncmp(argv[i], "proxy:", 6))
        {
            memset(http_proxy, 0, sizeof(http_proxy));
            strncpy(http_proxy, argv[i]+6, 255);
        }
        else if(!strncmp(argv[i], "nohud", 5))
        {
            hud_enable = 0;
        }
    }

    save_presets(0);

    make_kernel();
    prepare_alpha();

    stamp_init();

    sdl_open();
    http_init(http_proxy[0] ? http_proxy : NULL);

    if(cpu_check())
    {
        error_ui(vid_buf, 0, "Unsupported CPU. Try another version.");
        return 1;
    }

#ifdef BETA
    http_ver_check = http_async_req_start(NULL, "http://" SERVER "/Update.api?Action=CheckVersion", NULL, 0, 0);
#else
    http_ver_check = http_async_req_start(NULL, "http://" SERVER "/Update.api?Action=CheckVersion", NULL, 0, 0);
#endif

    while(!sdl_poll())
    {
        for(i=0; i<YRES/CELL; i++)
        {
            pv[i][0] = pv[i][0]*0.8f;
            pv[i][1] = pv[i][1]*0.8f;
            pv[i][2] = pv[i][2]*0.8f;
            pv[i][XRES/CELL-2] = pv[i][XRES/CELL-2]*0.8f;
            pv[i][XRES/CELL-1] = pv[i][XRES/CELL-1]*0.8f;
            vx[i][0] = vx[i][1]*0.9f;
            vx[i][1] = vx[i][2]*0.9f;
            vx[i][XRES/CELL-2] = vx[i][XRES/CELL-3]*0.9f;
            vx[i][XRES/CELL-1] = vx[i][XRES/CELL-2]*0.9f;
            vy[i][0] = vy[i][1]*0.9f;
            vy[i][1] = vy[i][2]*0.9f;
            vy[i][XRES/CELL-2] = vy[i][XRES/CELL-3]*0.9f;
            vy[i][XRES/CELL-1] = vy[i][XRES/CELL-2]*0.9f;
        }
        for(i=0; i<XRES/CELL; i++)
        {
            pv[0][i] = pv[0][i]*0.8f;
            pv[1][i] = pv[1][i]*0.8f;
            pv[2][i] = pv[2][i]*0.8f;
            pv[YRES/CELL-2][i] = pv[YRES/CELL-2][i]*0.8f;
            pv[YRES/CELL-1][i] = pv[YRES/CELL-1][i]*0.8f;
            vx[0][i] = vx[1][i]*0.9f;
            vx[1][i] = vx[2][i]*0.9f;
            vx[YRES/CELL-2][i] = vx[YRES/CELL-3][i]*0.9f;
            vx[YRES/CELL-1][i] = vx[YRES/CELL-2][i]*0.9f;
            vy[0][i] = vy[1][i]*0.9f;
            vy[1][i] = vy[2][i]*0.9f;
            vy[YRES/CELL-2][i] = vy[YRES/CELL-3][i]*0.9f;
            vy[YRES/CELL-1][i] = vy[YRES/CELL-2][i]*0.9f;
        }

        for(j=1; j<YRES/CELL; j++)
        {
            for(i=1; i<XRES/CELL; i++)
            {
                if(bmap[j][i]==1 || bmap[j][i]==8 || (bmap[j][i]==7 && !emap[j][i]))
                {
                    vx[j][i] = 0.0f;
                    vx[j][i-1] = 0.0f;
                    vy[j][i] = 0.0f;
                    vy[j-1][i] = 0.0f;
                }
            }
        }

        if(!sys_pause||framerender)
        {
#ifdef MT
            if(numCores>2)
            {
                pthread_t pth;
                pthread_create(&pth,NULL,update_air_th,"");
            }
            else
            {
                update_air();
            }
#else
            update_air();
#endif
        }
        if(cmode==0 || cmode==1)
        {
            draw_air(vid_buf);
        }
        else if(cmode==2)
        {
            memcpy(vid_buf, fire_bg, XRES*YRES*PIXELSIZE);
            memset(vid_buf+(XRES*YRES), 0, ((XRES+BARSIZE)*YRES*PIXELSIZE)-(XRES*YRES*PIXELSIZE));
        }
        else
        {
            memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
        }
        update_particles(vid_buf);
		draw_parts(vid_buf);

        if(cmode==2)
        {
            if(!fire_fc)
            {
                dim_copy(fire_bg, vid_buf);
            }
            else
            {
                memcpy(fire_bg, vid_buf, XRES*YRES*PIXELSIZE);
            }
            fire_fc = (fire_fc+1) % 3;
        }
        if(cmode==3||cmode==4||cmode==6)
            render_fire(vid_buf);

        render_signs(vid_buf);

        memset(vid_buf+((XRES+BARSIZE)*YRES), 0, (PIXELSIZE*(XRES+BARSIZE))*MENUSIZE);
        clearrect(vid_buf, XRES-1, 0, BARSIZE+1, YRES);

        draw_svf_ui(vid_buf);

        if(http_ver_check)
        {
            if(!do_check && http_async_req_status(http_ver_check))
            {
                ver_data = http_async_req_stop(http_ver_check, &http_ret, NULL);
                if(http_ret==200 && ver_data)
                {
#ifdef BETA
                    if(sscanf(ver_data, "%d.%d.%d", &major, &minor, &is_beta)==3)
                        if(major>SAVE_VERSION || (major==SAVE_VERSION && minor>MINOR_VERSION) || (major==SAVE_VERSION && is_beta == 0))
                            old_version = 1;
#else
                    if(sscanf(ver_data, "%d.%d", &major, &minor)==2)
                        if(major>SAVE_VERSION || (major==SAVE_VERSION && minor>MINOR_VERSION))
                            old_version = 1;
#endif
                    free(ver_data);
                }
                http_ver_check = NULL;
            }
            do_check = (do_check+1) & 15;
        }

        if(sdl_key=='q' || sdl_key==SDLK_ESCAPE)
        {
            if(confirm_ui(vid_buf, "You are about to quit", "Are you sure you want to quit?", "Quit"))
            {
                break;
            }
        }
		if(sdl_key=='d')
		{
			death = !(death);
		}
		if(sdl_key=='f')
		{
			framerender = 1;
		}
        if((sdl_key=='l' || sdl_key=='k') && stamps[0].name[0])
        {
            if(load_mode)
            {
                free(load_img);
                free(load_data);
                load_mode = 0;
                load_data = NULL;
                load_img = NULL;
            }
            if(it > 50)
                it = 50;
            if(sdl_key=='k' && stamps[1].name[0])
            {
                j = stamp_ui(vid_buf);
                if(j>=0)
                    load_data = stamp_load(j, &load_size);
                else
                    load_data = NULL;
            }
            else
                load_data = stamp_load(0, &load_size);
            if(load_data)
            {
                load_img = prerender_save(load_data, load_size, &load_w, &load_h);
                if(load_img)
                    load_mode = 1;
                else
                    free(load_data);
            }
        }
        if(sdl_key=='s')
        {
            if(it > 50)
                it = 50;
            save_mode = 1;
        }
        if(sdl_key=='1')
        {
            set_cmode(0);
        }
        if(sdl_key=='2')
        {
            set_cmode(1);
        }
        if(sdl_key=='3')
        {
            set_cmode(2);
        }
        if(sdl_key=='4')
        {
            set_cmode(3);
        }
        if(sdl_key=='5')
        {
            set_cmode(4);
        }
        if(sdl_key=='6')
        {
            set_cmode(5);
        }
        if(sdl_key=='7')
        {
            set_cmode(6);
        }
        if(sdl_key==SDLK_SPACE)
            sys_pause = !sys_pause;
        if(sdl_key=='h')
            hud_enable = !hud_enable;
        if(sdl_key=='p')
            dump_frame(vid_buf, XRES, YRES, XRES);
        if(sdl_key=='v'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
        {
            if(clipboard_ready==1)
            {
                load_data = malloc(clipboard_length);
                memcpy(load_data, clipboard_data, clipboard_length);
                load_size = clipboard_length;
                if(load_data)
                {
                    load_img = prerender_save(load_data, load_size, &load_w, &load_h);
                    if(load_img)
                        load_mode = 1;
                    else
                        free(load_data);
                }
            }
        }
        if(sdl_key=='x'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
        {
            save_mode = 1;
            copy_mode = 2;
        }
        if(sdl_key=='c'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
        {
            save_mode = 1;
            copy_mode = 1;
        }
        else if(sdl_key=='c')
        {
            set_cmode((cmode+1) % 7);
            if(it > 50)
                it = 50;
        }
        if(sdl_key=='z'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))) // Undo
        {
            int cbx, cby, cbi;

            for(cbi=0; cbi<NPART; cbi++)
                parts[cbi] = cb_parts[cbi];

            for(cby = 0; cby<YRES; cby++)
                for(cbx = 0; cbx<XRES; cbx++)
                    pmap[cby][cbx] = cb_pmap[cby][cbx];

            for(cby = 0; cby<(YRES/CELL); cby++)
                for(cbx = 0; cbx<(XRES/CELL); cbx++)
                {
                    vx[cby][cbx] = cb_vx[cby][cbx];
                    vy[cby][cbx] = cb_vy[cby][cbx];
                    pv[cby][cbx] = cb_pv[cby][cbx];
                    bmap[cby][cbx] = cb_bmap[cby][cbx];
                    emap[cby][cbx] = cb_emap[cby][cbx];
                }
        }
#ifdef INTERNAL
        if(sdl_key=='v')
            vs = !vs;
        if(vs)
            dump_frame(vid_buf, XRES, YRES, XRES);
#endif

        if(sdl_wheel)
        {
            if(sdl_zoom_trig==1)
            {
                ZSIZE += sdl_wheel;
                if(ZSIZE>32)
                    ZSIZE = 32;
                if(ZSIZE<2)
                    ZSIZE = 2;
                ZFACTOR = 256/ZSIZE;
                sdl_wheel = 0;
            }
            else
            {
                bs += sdl_wheel;
                if(bs>1224)
                    bs = 1224;
                if(bs<0)
                    bs = 0;
                sdl_wheel = 0;
                /*if(su >= PT_NUM) {
                	if(sl < PT_NUM)
                		su = sl;
                	if(sr < PT_NUM)
                		su = sr;
                }*/
            }
        }

        bq = b;
        b = SDL_GetMouseState(&x, &y);

        for(i=0; i<SC_TOTAL; i++)
        {
            draw_menu(vid_buf, i, active_menu);
        }

        for(i=0; i<SC_TOTAL; i++)
        {
            if(!b&&x>=sdl_scale*(XRES-2) && x<sdl_scale*(XRES+BARSIZE-1) &&y>= sdl_scale*((i*16)+YRES+MENUSIZE-16-(SC_TOTAL*16)) && y<sdl_scale*((i*16)+YRES+MENUSIZE-16-(SC_TOTAL*16)+15))
            {
                active_menu = i;
            }
        }
        menu_ui_v3(vid_buf, active_menu, &sl, &sr, b, bq, x, y);

        if(zoom_en && x>=sdl_scale*zoom_wx && y>=sdl_scale*zoom_wy
                && x<sdl_scale*(zoom_wx+ZFACTOR*ZSIZE)
                && y<sdl_scale*(zoom_wy+ZFACTOR*ZSIZE))
        {
            x = (((x/sdl_scale-zoom_wx)/ZFACTOR)+zoom_x)*sdl_scale;
            y = (((y/sdl_scale-zoom_wy)/ZFACTOR)+zoom_y)*sdl_scale;
        }
        if(y>0 && y<sdl_scale*YRES && x>0 && x<sdl_scale*XRES)
        {
            int cr;
            cr = pmap[y/sdl_scale][x/sdl_scale];
            if(!((cr>>8)>=NPART || !cr))
            {
#ifdef BETA
                sprintf(heattext, "%s, Pressure: %3.2f, Temp: %4.2f C, Life: %d", ptypes[cr&0xFF].name, pv[(y/sdl_scale)/CELL][(x/sdl_scale)/CELL], parts[cr>>8].temp, parts[cr>>8].life);
#else
                sprintf(heattext, "%s, Pressure: %3.2f, Temp: %4.2f C", ptypes[cr&0xFF].name, pv[(y/sdl_scale)/CELL][(x/sdl_scale)/CELL], parts[cr>>8].temp);
#endif
            }
            else
            {
                sprintf(heattext, "Empty, Pressure: %3.2f", pv[(y/sdl_scale)/CELL][(x/sdl_scale)/CELL]);
            }
        }
        mx = x;
        my = y;
        if(update_flag)
        {
            info_box(vid_buf, "Finalizing update...");
            if(last_major>SAVE_VERSION || (last_major==SAVE_VERSION && last_minor>=MINOR_VERSION))
            {
                update_cleanup();
                error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
            }
            else
            {
                if(update_finish())
                    error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
                else
                    info_ui(vid_buf, "Update success", "You have successfully updated the Powder Toy!");
            }
            update_flag = 0;
        }

        if(b && !bq && x>=(XRES-19-old_ver_len)*sdl_scale &&
                x<=(XRES-14)*sdl_scale && y>=(YRES-22)*sdl_scale && y<=(YRES-9)*sdl_scale && old_version)
        {
            tmp = malloc(64);
#ifdef BETA
            if(is_beta)
            {
                sprintf(tmp, "Your version: %d (Beta %d), new version: %d (Beta %d).", SAVE_VERSION, MINOR_VERSION, major, minor);
            }
            else
            {
                sprintf(tmp, "Your version: %d (Beta %d), new version: %d.%d.", SAVE_VERSION, MINOR_VERSION, major, minor);
            }
#else
            sprintf(tmp, "Your version: %d.%d, new version: %d.%d.", SAVE_VERSION, MINOR_VERSION, major, minor);
#endif
            if(confirm_ui(vid_buf, "Do you want to update The Powder Toy?", tmp, "Update"))
            {
                free(tmp);
                tmp = download_ui(vid_buf, my_uri, &i);
                if(tmp)
                {
                    save_presets(1);
                    if(update_start(tmp, i))
                    {
                        update_cleanup();
                        save_presets(0);
                        error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
                    }
                    else
                        return 0;
                }
            }
            else
                free(tmp);
        }
        if(y>=sdl_scale*(YRES+(MENUSIZE-20)))
        {
            if(x>=189*sdl_scale && x<=202*sdl_scale && svf_login && svf_open && svf_myvote==0)
            {
                db = svf_own ? 275 : 272;
                if(da < 51)
                    da ++;
            }
            else if(x>=204 && x<=217 && svf_login && svf_open && svf_myvote==0)
            {
                db = svf_own ? 275 : 272;
                if(da < 51)
                    da ++;
            }
            else if(x>=189 && x<=217 && svf_login && svf_open && svf_myvote!=0)
            {
                db = (svf_myvote==1) ? 273 : 274;
                if(da < 51)
                    da ++;
            }
            else if(x>=219*sdl_scale && x<=((XRES+BARSIZE-(510-349))*sdl_scale) && svf_login && svf_open)
            {
                db = svf_own ? 257 : 256;
                if(da < 51)
                    da ++;
            }
            else if(x>=((XRES+BARSIZE-(510-351))*sdl_scale) && x<((XRES+BARSIZE-(510-366))*sdl_scale))
            {
                db = 270;
                if(da < 51)
                    da ++;
            }
            else if(x>=((XRES+BARSIZE-(510-367))*sdl_scale) && x<((XRES+BARSIZE-(510-383))*sdl_scale))
            {
                db = 266;
                if(da < 51)
                    da ++;
            }
            else if(x>=37*sdl_scale && x<=187*sdl_scale && svf_login)
            {
                db = 259;
                if(svf_open && svf_own && x<=55*sdl_scale)
                    db = 258;
                if(da < 51)
                    da ++;
            }
            else if(x>=((XRES+BARSIZE-(510-385))*sdl_scale) && x<=((XRES+BARSIZE-(510-476))*sdl_scale))
            {
                db = svf_login ? 261 : 260;
                if(svf_admin)
                {
                    db = 268;
                }
                else if(svf_mod)
                {
                    db = 271;
                }
                if(da < 51)
                    da ++;
            }
            else if(x>=sdl_scale && x<=17*sdl_scale)
            {
                db = 262;
                if(da < 51)
                    da ++;
            }
            else if(x>=((XRES+BARSIZE-(510-494))*sdl_scale) && x<=((XRES+BARSIZE-(510-509))*sdl_scale))
            {
                db = sys_pause ? 264 : 263;
                if(da < 51)
                    da ++;
            }
            else if(x>=((XRES+BARSIZE-(510-476))*sdl_scale) && x<=((XRES+BARSIZE-(510-491))*sdl_scale))
            {
                db = 267;
                if(da < 51)
                    da ++;
            }
            else if(x>=19*sdl_scale && x<=35*sdl_scale && svf_open)
            {
                db = 265;
                if(da < 51)
                    da ++;
            }
            else if(da > 0)
                da --;
        }
        else if(da > 0)
            da --;

        if(!sdl_zoom_trig && zoom_en==1)
            zoom_en = 0;

        if(sdl_key==Z_keysym && zoom_en==2)
            zoom_en = 1;

        if(load_mode)
        {
            load_x = CELL*((mx/sdl_scale-load_w/2+CELL/2)/CELL);
            load_y = CELL*((my/sdl_scale-load_h/2+CELL/2)/CELL);
            if(load_x+load_w>XRES) load_x=XRES-load_w;
            if(load_y+load_h>YRES) load_y=YRES-load_h;
            if(load_x<0) load_x=0;
            if(load_y<0) load_y=0;
            if(bq==1 && !b)
            {
                parse_save(load_data, load_size, 0, load_x, load_y);
                free(load_data);
                free(load_img);
                load_mode = 0;
            }
            else if(bq==4 && !b)
            {
                free(load_data);
                free(load_img);
                load_mode = 0;
            }
        }
        else if(save_mode==1)
        {
            save_x = (mx/sdl_scale)/CELL;
            save_y = (my/sdl_scale)/CELL;
            if(save_x >= XRES/CELL) save_x = XRES/CELL-1;
            if(save_y >= YRES/CELL) save_y = YRES/CELL-1;
            save_w = 1;
            save_h = 1;
            if(b==1)
            {
                save_mode = 2;
            }
            else if(b==4)
            {
                save_mode = 0;
                copy_mode = 0;
            }
        }
        else if(save_mode==2)
        {
            save_w = (mx/sdl_scale+CELL/2)/CELL - save_x;
            save_h = (my/sdl_scale+CELL/2)/CELL - save_y;
            if(save_w>XRES/CELL) save_w = XRES/CELL;
            if(save_h>YRES/CELL) save_h = YRES/CELL;
            if(save_w<1) save_w = 1;
            if(save_h<1) save_h = 1;
            if(!b)
            {
                if(copy_mode==1)
                {
                    clipboard_data=build_save(&clipboard_length, save_x*CELL, save_y*CELL, save_w*CELL, save_h*CELL);
                    clipboard_ready = 1;
                    save_mode = 0;
                    copy_mode = 0;
                }
                else if(copy_mode==2)
                {
                    clipboard_data=build_save(&clipboard_length, save_x*CELL, save_y*CELL, save_w*CELL, save_h*CELL);
                    clipboard_ready = 1;
                    save_mode = 0;
                    copy_mode = 0;
                    clear_area(save_x*CELL, save_y*CELL, save_w*CELL, save_h*CELL);
                }
                else
                {
                    stamp_save(save_x*CELL, save_y*CELL, save_w*CELL, save_h*CELL);
                    save_mode = 0;
                }
            }
        }
        else if(sdl_zoom_trig && zoom_en<2)
        {
            x /= sdl_scale;
            y /= sdl_scale;
            x -= ZSIZE/2;
            y -= ZSIZE/2;
            if(x<0) x=0;
            if(y<0) y=0;
            if(x>XRES-ZSIZE) x=XRES-ZSIZE;
            if(y>YRES-ZSIZE) y=YRES-ZSIZE;
            zoom_x = x;
            zoom_y = y;
            zoom_wx = (x<XRES/2) ? XRES-ZSIZE*ZFACTOR : 0;
            zoom_wy = 0;
            zoom_en = 1;
            if(!b && bq)
                zoom_en = 2;
        }
        else if(b)
        {
            if(it > 50)
                it = 50;
            x /= sdl_scale;
            y /= sdl_scale;
            if(y>=YRES+(MENUSIZE-20))
            {
                if(!lb)
                {
                    if(x>=189 && x<=202 && svf_login && svf_open && svf_myvote==0 && svf_own==0)
                    {
                        if(execute_vote(vid_buf, svf_id, "Up"))
                        {
                            svf_myvote = 1;
                        }
                    }
                    if(x>=204 && x<=217 && svf_login && svf_open && svf_myvote==0 && svf_own==0)
                    {
                        if(execute_vote(vid_buf, svf_id, "Down"))
                        {
                            svf_myvote = -1;
                        }
                    }
                    if(x>=219 && x<=(XRES+BARSIZE-(510-349)) && svf_login && svf_open)
                        tag_list_ui(vid_buf);
                    if(x>=(XRES+BARSIZE-(510-351)) && x<(XRES+BARSIZE-(510-366)) && !bq)
                    {
                        legacy_enable = !legacy_enable;
                    }
                    if(x>=(XRES+BARSIZE-(510-367)) && x<=(XRES+BARSIZE-(510-383)) && !bq)
                    {
                        memset(signs, 0, sizeof(signs));
                        memset(pv, 0, sizeof(pv));
                        memset(vx, 0, sizeof(vx));
                        memset(vy, 0, sizeof(vy));
                        memset(fvx, 0, sizeof(fvx));
                        memset(fvy, 0, sizeof(fvy));
                        memset(bmap, 0, sizeof(bmap));
                        memset(emap, 0, sizeof(emap));
                        memset(parts, 0, sizeof(particle)*NPART);
                        for(i=0; i<NPART-1; i++)
                            parts[i].life = i+1;
                        parts[NPART-1].life = -1;
                        pfree = 0;

                        legacy_enable = 0;
                        svf_myvote = 0;
                        svf_open = 0;
                        svf_publish = 0;
                        svf_own = 0;
                        svf_id[0] = 0;
                        svf_name[0] = 0;
                        svf_tags[0] = 0;

                        memset(fire_bg, 0, XRES*YRES*PIXELSIZE);
                        memset(fire_r, 0, sizeof(fire_r));
                        memset(fire_g, 0, sizeof(fire_g));
                        memset(fire_b, 0, sizeof(fire_b));
                    }
                    if(x>=(XRES+BARSIZE-(510-385)) && x<=(XRES+BARSIZE-(510-476)))
                    {
                        login_ui(vid_buf);
                        if(svf_login)
                            save_presets(0);
                    }
                    if(x>=37 && x<=187 && svf_login)
                    {
                        if(!svf_open || !svf_own || x>51)
                        {
                            if(save_name_ui(vid_buf))
                                execute_save(vid_buf);
                        }
                        else
                            execute_save(vid_buf);
                        while(!sdl_poll())
                            if(!SDL_GetMouseState(&x, &y))
                                break;
                        b = bq = 0;
                    }
                    if(x>=1 && x<=17)
                    {
                        search_ui(vid_buf);
                        memset(fire_bg, 0, XRES*YRES*PIXELSIZE);
                        memset(fire_r, 0, sizeof(fire_r));
                        memset(fire_g, 0, sizeof(fire_g));
                        memset(fire_b, 0, sizeof(fire_b));
                    }
                    if(x>=19 && x<=35 && svf_last && svf_open)
                        parse_save(svf_last, svf_lsize, 1, 0, 0);
                    if(x>=(XRES+BARSIZE-(510-476)) && x<=(XRES+BARSIZE-(510-491)) && !bq)
                    {
                        if(b & SDL_BUTTON_LMASK)
                            set_cmode((cmode+1) % 7);
                        if(b & SDL_BUTTON_RMASK)
                            set_cmode((cmode+6) % 7);
                        save_presets(0);
                    }
                    if(x>=(XRES+BARSIZE-(510-494)) && x<=(XRES+BARSIZE-(510-509)) && !bq)
                        sys_pause = !sys_pause;
                    lb = 0;
                }
            }
            else if(y<YRES)
            {
                c = (b&1) ? sl : sr;
                su = c;
                if(c==126)
                {
                    if(!bq)
                        add_sign_ui(vid_buf, x, y);
                }
                else if(lb)
                {
                    if(lm == 1)
                    {
                        xor_line(lx, ly, x, y, vid_buf);
                        if(c==127 && lx>=0 && ly>=0 && lx<XRES && ly<YRES && bmap[ly/CELL][lx/CELL]==4)
                        {
                            nfvx = (x-lx)*0.005f;
                            nfvy = (y-ly)*0.005f;
                            flood_parts(lx, ly, 255, -1, 4);
                            for(j=0; j<YRES/CELL; j++)
                                for(i=0; i<XRES/CELL; i++)
                                    if(bmap[j][i] == 255)
                                    {
                                        fvx[j][i] = nfvx;
                                        fvy[j][i] = nfvy;
                                        bmap[j][i] = 4;
                                    }
                        }
                    }
                    else if(lm == 2)
                    {
                        xor_line(lx, ly, lx, y, vid_buf);
                        xor_line(lx, y, x, y, vid_buf);
                        xor_line(x, y, x, ly, vid_buf);
                        xor_line(x, ly, lx, ly, vid_buf);
                    }
                    else
                    {
                        create_line(lx, ly, x, y, bs, c);
                        lx = x;
                        ly = y;
                    }
                }
                else
                {
                    if((sdl_mod & (KMOD_LSHIFT|KMOD_RSHIFT)) && !(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
                    {
                        lx = x;
                        ly = y;
                        lb = b;
                        lm = 1;
                    }
                    else if((sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)) && !(sdl_mod & (KMOD_LSHIFT|KMOD_RSHIFT)))
                    {
                        lx = x;
                        ly = y;
                        lb = b;
                        lm = 2;
                    }
                    else if((sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)) && (sdl_mod & (KMOD_LSHIFT|KMOD_RSHIFT)))
                    {
                        if(c!=125&&c!=SPC_AIR&&c!=SPC_HEAT&&c!=SPC_COOL&&c!=SPC_VACUUM)
                            flood_parts(x, y, c, -1, -1);
                        lx = x;
                        ly = y;
                        lb = 0;
                        lm = 0;
                    }
                    else if((sdl_mod & (KMOD_LALT||KMOD_RALT)) || b==SDL_BUTTON_MIDDLE)
                    {
                        if(y>0 && y<sdl_scale*YRES && x>0 && x<sdl_scale*XRES)
                        {
                            int cr;
                            cr = pmap[y][x];
                            if(!((cr>>8)>=NPART || !cr))
                            {
                                c = sl = cr&0xFF;
                            }
                            else
                            {
                                //Something
                            }
                        }
                        lx = x;
                        ly = y;
                        lb = 0;
                        lm = 0;
                    }
                    else
                    {
                        //Copy state before drawing any particles (for undo)7
                        int cbx, cby, cbi;

                        for(cbi=0; cbi<NPART; cbi++)
                            cb_parts[cbi] = parts[cbi];

                        for(cby = 0; cby<YRES; cby++)
                            for(cbx = 0; cbx<XRES; cbx++)
                                cb_pmap[cby][cbx] = pmap[cby][cbx];

                        for(cby = 0; cby<(YRES/CELL); cby++)
                            for(cbx = 0; cbx<(XRES/CELL); cbx++)
                            {
                                cb_vx[cby][cbx] = vx[cby][cbx];
                                cb_vy[cby][cbx] = vy[cby][cbx];
                                cb_pv[cby][cbx] = pv[cby][cbx];
                                cb_bmap[cby][cbx] = bmap[cby][cbx];
                                cb_emap[cby][cbx] = emap[cby][cbx];
                            }

                        create_parts(x, y, bs, c);
                        lx = x;
                        ly = y;
                        lb = b;
                        lm = 0;
                    }
                }
            }
        }
        else
        {
            if(lb && lm)
            {
                x /= sdl_scale;
                y /= sdl_scale;
                c = (lb&1) ? sl : sr;
                su = c;
                if(lm == 1)
                {
                    if(c!=127 || lx<0 || ly<0 || lx>=XRES || ly>=YRES || bmap[ly/CELL][lx/CELL]!=4)
                        create_line(lx, ly, x, y, bs, c);
                }
                else
                    create_box(lx, ly, x, y, c);
                lm = 0;
            }
            lb = 0;
        }

        if(load_mode)
        {
            draw_image(vid_buf, load_img, load_x, load_y, load_w, load_h, 128);
            xor_rect(vid_buf, load_x, load_y, load_w, load_h);
        }

        if(save_mode)
        {
            xor_rect(vid_buf, save_x*CELL, save_y*CELL, save_w*CELL, save_h*CELL);
            da = 51;
            db = 269;
        }

        if(zoom_en!=1 && !load_mode && !save_mode)
        {
            render_cursor(vid_buf, mx/sdl_scale, my/sdl_scale, su, bs);
            mousex = mx/sdl_scale;
            mousey = my/sdl_scale;
        }

        if(zoom_en)
            render_zoom(vid_buf);

        if(da)
            switch(db)
            {
            case 256:
                drawtext(vid_buf, 16, YRES-24, "Add simulation tags.", 255, 255, 255, da*5);
                break;
            case 257:
                drawtext(vid_buf, 16, YRES-24, "Add and remove simulation tags.", 255, 255, 255, da*5);
                break;
            case 258:
                drawtext(vid_buf, 16, YRES-24, "Save the simulation under the current name.", 255, 255, 255, da*5);
                break;
            case 259:
                drawtext(vid_buf, 16, YRES-24, "Save the simulation under a new name.", 255, 255, 255, da*5);
                break;
            case 260:
                drawtext(vid_buf, 16, YRES-24, "Sign into the Simulation Server.", 255, 255, 255, da*5);
                break;
            case 261:
                drawtext(vid_buf, 16, YRES-24, "Sign into the Simulation Server under a new name.", 255, 255, 255, da*5);
                break;
            case 262:
                drawtext(vid_buf, 16, YRES-24, "Find & open a simulation", 255, 255, 255, da*5);
                break;
            case 263:
                drawtext(vid_buf, 16, YRES-24, "Pause the simulation", 255, 255, 255, da*5);
                break;
            case 264:
                drawtext(vid_buf, 16, YRES-24, "Resume the simulation", 255, 255, 255, da*5);
                break;
            case 265:
                drawtext(vid_buf, 16, YRES-24, "Reload the simulation", 255, 255, 255, da*5);
                break;
            case 266:
                drawtext(vid_buf, 16, YRES-24, "Erase all particles and walls", 255, 255, 255, da*5);
                break;
            case 267:
                drawtext(vid_buf, 16, YRES-24, "Change display mode", 255, 255, 255, da*5);
                break;
            case 268:
                drawtext(vid_buf, 16, YRES-24, "Annuit C\245ptis", 255, 255, 255, da*5);
                break;
            case 269:
                drawtext(vid_buf, 16, YRES-24, "Click-and-drag to specify a rectangle to copy (right click = cancel).", 255, 216, 32, da*5);
                break;
            case 270:
                drawtext(vid_buf, 16, YRES-24, "Enable or disable compatability mode (disables heat simulation).", 255, 255, 255, da*5);
                break;
            case 271:
                drawtext(vid_buf, 16, YRES-24, "You're a moderator", 255, 255, 255, da*5);
                break;
            case 272:
                drawtext(vid_buf, 16, YRES-24, "Like/Dislike this save.", 255, 255, 255, da*5);
                break;
            case 273:
                drawtext(vid_buf, 16, YRES-24, "You like this.", 255, 255, 255, da*5);
                break;
            case 274:
                drawtext(vid_buf, 16, YRES-24, "You dislike this.", 255, 255, 255, da*5);
                break;
            case 275:
                drawtext(vid_buf, 16, YRES-24, "You cannot vote on your own save.", 255, 255, 255, da*5);
                break;
            default:
                drawtext(vid_buf, 16, YRES-24, (char *)ptypes[db].descs, 255, 255, 255, da*5);
            }
        if(itc)
        {
            itc--;
            drawtext(vid_buf, (XRES-textwidth(itc_msg))/2, ((YRES/2)-10), itc_msg, 255, 255, 255, itc>51?255:itc*5);
        }
        if(it)
        {
            it--;
            drawtext(vid_buf, 16, 20, it_msg, 255, 255, 255, it>51?255:it*5);
        }

        if(old_version)
        {
            clearrect(vid_buf, XRES-21-old_ver_len, YRES-24, old_ver_len+9, 17);
#ifdef BETA
            if(is_beta)
            {
                drawtext(vid_buf, XRES-16-old_ver_len, YRES-19, old_ver_msg_beta, 255, 216, 32, 255);
            }
            else
            {
                drawtext(vid_buf, XRES-16-old_ver_len, YRES-19, old_ver_msg, 255, 216, 32, 255);
            }
#else
            drawtext(vid_buf, XRES-16-old_ver_len, YRES-19, old_ver_msg, 255, 216, 32, 255);
#endif
            drawrect(vid_buf, XRES-19-old_ver_len, YRES-22, old_ver_len+5, 13, 255, 216, 32, 255);
        }

        if(hud_enable)
        {
            currentTime = SDL_GetTicks();
            if(currentTime-past>=16)
            {
                past = SDL_GetTicks();
                FPS++;
            }
            if(currentTime-pastFPS>=1000)
            {
#ifdef BETA
                sprintf(uitext, "Version %d (Beta %d) FPS:%d", SAVE_VERSION, MINOR_VERSION, FPS);
#else
                sprintf(uitext, "Version %d.%d FPS:%d", SAVE_VERSION, MINOR_VERSION, FPS);
#endif
                FPSB = FPS;
                FPS = 0;
                pastFPS = currentTime;
            }
            if(sdl_zoom_trig||zoom_en)
            {
                if(zoom_x<XRES/2)
                {
                    fillrect(vid_buf, XRES-20-textwidth(heattext), 266, textwidth(heattext)+8, 15, 0, 0, 0, 140);
                    drawtext(vid_buf, XRES-16-textwidth(heattext), 270, heattext, 255, 255, 255, 200);
                }
                else
                {
                    fillrect(vid_buf, 12, 266, textwidth(heattext)+8, 15, 0, 0, 0, 140);
                    drawtext(vid_buf, 16, 270, heattext, 255, 255, 255, 200);
                }
            }
            else
            {
                fillrect(vid_buf, XRES-20-textwidth(heattext), 12, textwidth(heattext)+8, 15, 0, 0, 0, 140);
                drawtext(vid_buf, XRES-16-textwidth(heattext), 16, heattext, 255, 255, 255, 200);
            }
            fillrect(vid_buf, 12, 12, textwidth(uitext)+8, 15, 0, 0, 0, 140);
            drawtext(vid_buf, 16, 16, uitext, 32, 216, 255, 200);
        }
	sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);

	//Setting an element for the stick man
	if(ptypes[sr].falldown>0)
		player[2] = sr;
	else
		player[2] = PT_DUST;
    }

    http_done();
    return 0;
}

