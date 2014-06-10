#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>

#include <pthread.h>
#include <unistd.h>

#define DEFAULT_BDWIDTH 1 /* border width */

char *text = "Hello Xft";
Display *display;
int screen;
Window window;
GC gc;
Colormap cmap;
Visual *visual;

Pixmap bitmap[16];

XftFont *font[16], *sfont;
XftDraw *xftdraw;
XftColor xftcolor;

uint32_t scolor;

Atom XA_CLIPBOARD, targets;

Pixmap drawbuf;

void postmessage(uint32_t msg, uint16_t param1, uint16_t param2, void *data)
{
    XEvent event = {
        .xclient = {
            .type = ClientMessage,
            .message_type = msg,
            .format = 8,
            .data = {
                .s = {param1, param2}
            }
        }
    };

    memcpy(&event.xclient.data.s[2], &data, sizeof(void*));

    XSendEvent(display, window, False, 0,  &event);
    XFlush(display);
}

void drawbitmap(int bm, int x, int y, int width, int height)
{
    //debug("%u %u\n", bm, bitmap[bm]);
    if(bm <= BM_PLUS) {
        XCopyPlane(display, bitmap[bm], drawbuf, gc, 0, 0, width, height, x, y, 1);
    } else {
        XCopyArea(display, bitmap[bm], drawbuf, gc, 0, 0, width, height, x, y);
    }
}

void drawbitmaptrans(int bm, int x, int y, int width, int height)
{

}

void drawbitmapalpha(int bm, int x, int y, int width, int height)
{
    XCopyArea(display, bitmap[bm], drawbuf, gc, 0, 0, width, height, x, y);
}

void drawtext(int x, int y, uint8_t *str, uint16_t length)
{
    XftDrawStringUtf8(xftdraw, &xftcolor, sfont, x, y + sfont->ascent, str, length);
}

void drawtextW(int x, int y, char_t *str, uint16_t length)
{
    drawtext(x, y, str, length);
}

int drawtext_getwidth(int x, int y, uint8_t *str, uint16_t length)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, sfont, str, length, &extents);

    XftDrawStringUtf8(xftdraw, &xftcolor, sfont, x, y + sfont->ascent, str, length);

    return extents.xOff;
}

int drawtext_getwidthW(int x, int y, char_t *str, uint16_t length)
{
    return drawtext_getwidth(x, y, str, length);
}

void drawtextwidth(int x, int width, int y, uint8_t *str, uint16_t length)
{
    pushclip(x, y, width, 256);

    drawtext(x, y, str, length);

    popclip();
}

void drawtextwidth_right(int x, int width, int y, uint8_t *str, uint16_t length)
{
    drawtext(x, y, str, length);
}

void drawtextwidth_rightW(int x, int width, int y, char_t *str, uint16_t length)
{
    drawtext(x, y, str, length);
}

void drawtextrange(int x, int x2, int y, uint8_t *str, uint16_t length)
{
    drawtext(x, y, str, length);
}

void drawtextrangecut(int x, int x2, int y, uint8_t *str, uint16_t length)
{
    drawtext(x, y, str, length);
}

int textwidth(uint8_t *str, uint16_t length)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, sfont, str, length, &extents);

    return extents.xOff;
}

int textwidthW(char_t *str, uint16_t length)
{
    return textwidth(str, length);
}

int textfit(uint8_t *str, uint16_t length, int width)
{
    int i = 0;
    while(i < length) {
        i++;
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, sfont, str, i, &extents);
        if(extents.xOff >= width) {
            i--;
            break;
        }
    }

    return i;
}

int textfitW(char_t *str, uint16_t length, int width)
{
    return textfit(str, length, width);
}

void drawrect(int x, int y, int width, int height, uint32_t color)
{
    XSetForeground(display, gc, color);
    XFillRectangle(display, drawbuf, gc, x, y, width, height);
}

void drawhline(int x, int y, int x2, uint32_t color)
{
    XSetForeground(display, gc, color);
    XDrawLine(display, drawbuf, gc, x, y, x2, y);
}

void drawvline(int x, int y, int y2, uint32_t color)
{
    XSetForeground(display, gc, color);
    XDrawLine(display, drawbuf, gc, x, y, x, y2);
}

void fillrect(RECT *r, uint32_t color)
{
    XSetForeground(display, gc, color);
    XFillRectangle(display, drawbuf, gc, r->left, r->top, r->right - r->left + 1, r->bottom - r->top + 1);
}

void framerect(RECT *r, uint32_t color)
{
    XSetForeground(display, gc, color);
    XDrawRectangle(display, drawbuf, gc, r->left, r->top, r->right - r->left, r->bottom - r->top);
}

void setfont(int id)
{
    sfont = font[id];
}

uint32_t setcolor(uint32_t color)
{
    XRenderColor xrcolor;
    XftColorFree(display, visual, cmap, &xftcolor);
    xrcolor.red = ((color >> 8) & 0xFF00) | 0x80;
    xrcolor.green = ((color) & 0xFF00) | 0x80;
    xrcolor.blue = ((color << 8) & 0xFF00) | 0x80;
    xrcolor.alpha = 0xFFFF;
    XftColorAllocValue(display, visual, cmap, &xrcolor, &xftcolor);

    uint32_t old = scolor;
    scolor = color;
    xftcolor.pixel = color;
    XSetForeground(display, gc, color);
    return old;
}

void setbkcolor(uint32_t color)
{
    XSetBackground(display, gc, color);
}

void setbgcolor(uint32_t color)
{
    XSetBackground(display, gc, color);
}

//XRectangle clip[16];
//static int clipk;

void pushclip(int left, int top, int width, int height)
{
    /*if(!clipk) {
        XSetClipMask(display, gc, drawbuf);
    }

    XRectangle *r = &clip[clipk++];
    r->x = left;
    r->y = top;
    r->width = width;
    r->height = height;

    XSetClipRectangles(display, gc, 0, 0, r, 1, Unsorted);*/
}

void popclip(void)
{
    /*clipk--;
    if(!clipk) {
        XSetClipMask(display, gc, None);
        return;
    }

    XRectangle *r = &clip[clipk - 1];

    XSetClipRectangles(display, gc, 0, 0, r, 1, Unsorted);*/
}

void enddraw(int x, int y, int width, int height)
{
    XCopyArea(display, drawbuf, window, gc, x, y, width, height, x, y);
}

uint16_t utf8tonative(uint8_t *str, char_t *out, uint16_t length)
{
    memcpy(out, str, length);
    return length;
}

void thread(void func(void*), void *args)
{
    pthread_t thread_temp;
    pthread_create(&thread_temp, NULL, (void*(*)(void*))func, args);
}

void yieldcpu(void)
{
    usleep(1000);
}

uint64_t get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return ((uint64_t)ts.tv_sec * (1000 * 1000 * 1000)) + (uint64_t)ts.tv_nsec;
}


void address_to_clipboard(void)
{

}

void editpopup(void)
{

}

void listpopup(uint8_t item)
{

}

void openurl(char_t *str)
{
    char cmd[1024];
    sprintf(cmd, "xdg-open %s", str);
    debug("cmd: %s\n", cmd);
    system(cmd);
}

void openfilesend(void)
{

}

void savefilerecv(uint32_t fid, MSG_FILE *file)
{

}

void sysmexit(void)
{

}

void sysmsize(void)
{

}

void sysmmini(void)
{

}

void pasteprimary(void)
{
    Window owner = XGetSelectionOwner(display, XA_PRIMARY);
    if(owner) {
        XConvertSelection(display, XA_PRIMARY, XA_STRING, targets, window, CurrentTime);
    }
}

void pasteclipboard(void)
{
    Window owner = XGetSelectionOwner(display, XA_CLIPBOARD);
    if(owner) {
        XConvertSelection(display, XA_CLIPBOARD, XA_STRING, targets, window, CurrentTime);
    }
}

static Pixmap createbitmap(int width, int height, void *data)
{
    XGCValues gcvalues;

    XImage *img = XCreateImage(display, CopyFromParent, 24, ZPixmap, 0, data, width, height, 32, 0);
    Pixmap p = XCreatePixmap(display, window, width, height, 24);
    GC gc = XCreateGC(display, p, 0, &gcvalues);

    XPutImage(display, p, gc, img, 0, 0, 0, 0, width, height);

    return p;
}

int main(int argc, char *argv[])
{
    _Bool done = 0;

    XEvent event;

    XSizeHints xsh = {
        .flags = PSize | PMinSize,
        .width = 800,
        .min_width = 480,
        .height = 600,
        .min_height = 320
    };

    /*XWMHints xwmh = {
        .flags = InputHint | StateHint,
        .input = False,
        .initial_state = NormalState
    };*/

    XInitThreads();

    if((display = XOpenDisplay(NULL)) == NULL) {
        printf("Cannot open display\n");
        return 1;
    }


    thread(tox_thread, NULL);

    screen = DefaultScreen(display);
    cmap = DefaultColormap(display, screen);
    visual = DefaultVisual(display, screen);

    XA_CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
    targets = XInternAtom(display, "TARGETS", 0);

    /*int nvi = 0, x;
    XVisualInfo template;
    template.depth = 32;
    template.screen = screen;
    XVisualInfo *vlist = XGetVisualInfo(display, VisualDepthMask, &template, &nvi);

    for(x = 0; x < nvi; x++)
    {
        if(vlist[x].depth == 32)
            break;
    }

    visual = vlist[x].visual;*/

    XSetWindowAttributes attrib = {
        .background_pixel = WhitePixel(display, screen),
        .border_pixel = BlackPixel(display, screen),
        .event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
                    PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask,
    };


    font[FONT_TITLE] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 20.0, NULL);
    font[FONT_SUBTITLE] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 18.0, NULL);
    font[FONT_MED] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 16.0, NULL);
    font[FONT_TEXT_LARGE] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 14.0, NULL);
    font[FONT_TEXT] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 12.0, NULL);

    font[FONT_MSG] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 16.0, NULL);
    font[FONT_MSG_NAME] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 16.0, XFT_WEIGHT, XftTypeInteger, FC_WEIGHT_BOLD, NULL);
    font[FONT_MSG_LINK] = XftFontOpen(display, screen, XFT_FAMILY, XftTypeString, "DejaVu Sans", XFT_PIXEL_SIZE, XftTypeDouble, 16.0, NULL);


    font_small_lineheight = font[FONT_TEXT]->height;
    font_msg_lineheight = font[FONT_MSG]->height;

    //window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 800, 600, 0, BlackPixel(display, screen), WhitePixel(display, screen));
    window = XCreateWindow(display, RootWindow(display, screen), 0, 0, 800, 600, 0, 24, InputOutput, visual, CWBackPixel | CWBorderPixel | CWEventMask, &attrib);
    drawbuf = XCreatePixmap(display, window, 800, 600, 24);
    //XSelectInput(display, window, ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask);


    bitmap[BM_MINIMIZE] = XCreateBitmapFromData(display, window, (char*)bm_minimize_bits, 16, 10);
    bitmap[BM_RESTORE] = XCreateBitmapFromData(display, window, (char*)bm_restore_bits, 16, 10);
    bitmap[BM_MAXIMIZE] = XCreateBitmapFromData(display, window, (char*)bm_maximize_bits, 16, 10);
    bitmap[BM_EXIT] = XCreateBitmapFromData(display, window, (char*)bm_exit_bits, 16, 10);
    bitmap[BM_PLUS] = XCreateBitmapFromData(display, window, (char*)bm_plus_bits, 16, 16);

    bitmap[BM_ONLINE] = createbitmap(10, 10, bm_online_bits);
    bitmap[BM_AWAY] = createbitmap(10, 10, bm_away_bits);
    bitmap[BM_BUSY] = createbitmap(10, 10, bm_busy_bits);
    bitmap[BM_OFFLINE] = createbitmap(10, 10, bm_offline_bits);
    bitmap[BM_CONTACT] = createbitmap(48, 48, bm_contact_bits);
    bitmap[BM_GROUP] = createbitmap(48, 48, bm_group_bits);
    bitmap[BM_FILE] = createbitmap(48, 48, bm_file_bits);

    uint32_t test[64];
    int xx = 0;
    while(xx < 8) {
        int y = 0;
        while(y < 8) {
            uint32_t value = 0xFFFFFF;
            if(xx + y >= 7) {
                int a = xx % 3, b = y % 3;
                if(a == 1) {
                    if(b == 0) {
                        value = 0xB6B6B6;
                    } else if(b == 1) {
                        value = 0x999999;
                    }
                } else if(a == 0 && b == 1) {
                    value = 0xE0E0E0;
                }
            }

            test[y * 8 + xx] = value;
            y++;
        }
        xx++;
    }

    bitmap[BM_CORNER] = createbitmap(8, 8, test);

    XSetStandardProperties(display, window, "winTox", "winTox", None, argv, argc, &xsh);
    //XSetWMHints(display, window, &xwmh);

    /* Xft draw context */
    xftdraw = XftDrawCreate(display, drawbuf, visual, cmap);
    /* Xft text color */
    XRenderColor xrcolor;
    xrcolor.red = 0x0;
    xrcolor.green = 0x0;
    xrcolor.blue = 0x0;
    xrcolor.alpha = 0xffff;
    XftColorAllocValue(display, visual, cmap, &xrcolor, &xftcolor);

    XMapWindow(display, window);
    printf("click on the window to exit\n");

    gc = DefaultGC(display, screen);//XCreateGC(display, window, 0, 0);

    width = 800;
    height = 600;

    //load fonts
    //load bitmaps

    //wait for tox_thread init
    while(!tox_thread_init) {
        yieldcpu();
    }

    list_start();

    redraw();

    while(!done) {
        XNextEvent(display, &event);
        switch(event.type) {
        case Expose: {
            redraw();
            break;
        }

        case ConfigureNotify: {
            XConfigureEvent *ev = &event.xconfigure;
            width = ev->width;
            height = ev->height;

            XFreePixmap(display, drawbuf);
            drawbuf = XCreatePixmap(display, window, width, height, 24);

            XftDrawDestroy(xftdraw);
            xftdraw = XftDrawCreate(display, drawbuf, visual, cmap);

            redraw();
            break;
        }

        case LeaveNotify: {
            panel_mleave(&panel_main);
        }

        case MotionNotify: {
            XMotionEvent *ev = &event.xmotion;
            static int my;
            int dy;

            dy = ev->y - my;
            my = ev->y;

            hand = 0;

            panel_mmove(&panel_main, 0, 0, width, height, ev->x, ev->y, dy);

            //SetCursor(hand ? cursor_hand : cursor_arrow);

            //debug("MotionEvent: (%u %u) %u\n", ev->x, ev->y, ev->state);
            break;
        }

        case ButtonPress: {
            XButtonEvent *ev = &event.xbutton;
            switch(ev->button) {
            case Button1: {
                panel_mdown(&panel_main);
                mdown = 1;
                break;
            }

            case Button2: {
                pasteprimary();
                break;
            }

            case Button3: {
                panel_mright(&panel_main);
                break;
            }
            }

            //debug("ButtonEvent: %u %u\n", ev->state, ev->button);
            break;
        }

        case ButtonRelease: {
            XButtonEvent *ev = &event.xbutton;
            switch(ev->button) {
            case Button1: {
                panel_mup(&panel_main);
                mdown = 0;
                break;
            }
            }
            break;
        }

        case KeyPress: {
            XKeyEvent *ev = &event.xkey;
            KeySym sym = XKeycodeToKeysym(display, ev->keycode, 0);

            //debug("KeyEvent: %u %u\n", ev->state, ev->keycode);

            if(ev->state & 4) {
                if(sym == 'v') {
                    pasteclipboard();
                }
                break;
            }

            if(edit_active()) {
                edit_char(sym);
                break;
            }

            break;
        }

        case SelectionNotify: {

            XSelectionEvent *ev = &event.xselection;

            if(ev->property == None) {
                break;
            }

            Atom type;
            int format;
            long unsigned int len, bytes_left;
            void *data;

            XGetWindowProperty(display, window, ev->property, 0, ~0L, True, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char**)&data);

            if(!data) {
                break;
            }

            if(edit_active()) {
                edit_paste(data, len);
            }

            XFree(data);

            break;
        }

        case ClientMessage:
            {
                XClientMessageEvent *ev = &event.xclient;
                void *data;
                memcpy(&data, &ev->data.s[2], sizeof(void*));
                tox_message(ev->message_type, ev->data.s[0], ev->data.s[1], data);
                break;
            }

        }
    }

    XFreePixmap(display, drawbuf);

    XftDrawDestroy(xftdraw);
    XftColorFree(display, visual, cmap, &xftcolor);

    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
