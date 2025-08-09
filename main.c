#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>

// Getting home directory
#include <unistd.h>
#include <pwd.h>

#include <raylib.h>
#define CLAY_IMPLEMENTATION
#include "clay.h"

/* Key configurations */
#define K_QUIT KEY_Q
#define K_HOME KEY_W
#define K_HIDE KEY_H
#define K_ACCEPT KEY_E
#define K_ACCEPT_SERIOUS KEY_ENTER
#define K_CANCEL KEY_ESCAPE
#define K_BACK KEY_A
#define K_REDO KEY_D
#define K_MAKEDIR KEY_F
#define K_SELECT KEY_S
#define K_DEL KEY_R
#define K_MOVE KEY_G
#define K_COPY KEY_T

/* Colorscheme and other UI configuration */
#define BOTTOM_BAR
#define INIT_SCRW 900
#define INIT_SCRH 600
#define FONT_SIZE 26
#define TEXT_SPACING 1
#define MOUSE_SCROLL_SENSITIVITY 5
#define CLR_BG {0, 0, 0, 255}
#define CLR_TXT {255,255,255,255}
#define CLR_SELECT {0, 255, 255, 255}
#define CLR_TXT_LNK {0, 150, 255, 255}
#define CLR_TXT_BLK {255, 255, 0, 255}
#define CLR_TXT_SCK {128, 0, 0, 255}
#define CLR_TXT_PIP {255, 0, 0, 255}
#define CLR_TXT_CHR {0, 255, 0, 255}
#define CLR_TXT_DIR {255, 0, 255, 255}
#define CLR_BG_TOPBAR {255, 255, 255, 255}
#define CLR_TXT_TOPBAR {0, 0, 0, 255}
#define CLR_BG_HOVER {255, 255, 255, 255}
#define CLR_TXT_HOVER {0, 0, 0, 255}
#define CLR_BG_TOPBAR_HOVER {0, 0, 0, 255}
#define CLR_TXT_TOPBAR_HOVER {255, 255, 255, 255}
#define CLR_TXT_TODEL {255, 0, 50, 255}
#define CLR_TXT_MAKEDIR CLR_TXT_DIR    

#define COLOR_CL2R(cc) (Color){.r=cc.r,.g=cc.g,.b=cc.b,.a=cc.a}
#define CLR_HOVER(c1,c2) (Clay_Hovered() ? (Clay_Color) c1 : (Clay_Color) c2)

//NOTE: this adds up to quite big
//memory usage (a few MBs). We need
//to optimize this.
#define CURPATH_SIZE 1024
#define MESSAGE_SIZE  256
#define TOFREE_SIZE 10000
#define TOFREE_NAMELEN 256
#define CHOSEN_SIZE 256

char curpath[CURPATH_SIZE];
char prevpath[CURPATH_SIZE];
char message[MESSAGE_SIZE];
char estrs[TOFREE_SIZE][TOFREE_NAMELEN] = {0};
char chosen[CHOSEN_SIZE] = {0};
bool hidemode = true;
bool choosemode = false;
int numfiles = 0;
// List of selected filepaths
char **selected = NULL;
size_t selected_sz = 0;

bool
isselect(const char *name) {
    char *path = malloc(strlen(curpath) + strlen(name) + 1);
    strcpy(path, curpath);
    strcat(path, name);

    for (size_t i = 0; i < selected_sz; i++) {
        if (!strcmp(path, selected[i])) {
            free(path);
            return true;
        }
    }

    free(path);
    return false;
}

void
doselect(const char *name) {
    if (isselect(name)) return;

    char *path = malloc(strlen(curpath) + strlen(name) + 1);
    strcpy(path, curpath);
    strcat(path, name);

    snprintf(message, MESSAGE_SIZE, "Selected %d", selected_sz+1);

    selected = realloc(selected, (selected_sz+1)*sizeof(char*));

    selected[selected_sz++] = path;
}

void
freeselect(void) {
    if (selected == NULL) return;
    for(--selected_sz; selected_sz; selected_sz--) {
        free(selected[selected_sz]);
    }
    free(selected);
    selected = NULL;
}

void
errorhandle(Clay_ErrorData errdat) {
    printf("[CLAY ERROR] %s\n", errdat.errorText.chars);
    exit(1);
}

static inline char *
slice2str(Clay_StringSlice slice) {
    char *str = malloc(slice.length + 1);
    *stpncpy(str, slice.chars, slice.length) = 0;
    return str;
}

static inline char *
clstr2str(Clay_String clstr) {
    char *str = malloc(clstr.length + 1);
    *stpncpy(str, clstr.chars, clstr.length) = 0;
    return str;
}

static inline Clay_Dimensions
measuretext(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    //FIXME: both of these methods return slightly incorrect results.
    //The first one, for some reason, returns more length than there is,
    //and the second one accumulates insufficience of length


    //// Note: only monospace fonts
    //Clay_Dimensions fuckyourmomcanyouprovideproperfuckinggoddamndocsqqq = (Clay_Dimensions) {
    //        .width = text.length * config->fontSize,
    //        .height = config->fontSize
    //};
    //return fuckyourmomcanyouprovideproperfuckinggoddamndocsqqq;
    
    const Font *font = (Font*)userData;
    char *str = slice2str(text);
    const Vector2 measure = MeasureTextEx(*font, slice2str(text), config->fontSize, TEXT_SPACING);
    free(str);

    return (Clay_Dimensions) {.width = measure.x, .height = measure.y};
}

void
explain(const char *what) {
    perror(what);
    strncpy(message, strerror(errno), MESSAGE_SIZE);
}

void
opencurdir(DIR **dirp) {
    *dirp = opendir(curpath);
    if (*dirp == NULL) {
        perror("opendir()");
        exit(1);
    }
}

void
closecurdir(DIR **dirp) {
    if (closedir(*dirp)) {
        perror("closedir()");
        exit(1);
    }
}

void
backhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata);


void
advance(DIR **dirp, const char *dest) {
    char *pathcpy = malloc(CURPATH_SIZE);
    strncpy(pathcpy, curpath, CURPATH_SIZE-1);

    strncat(pathcpy, dest, CURPATH_SIZE-1);
    strncat(pathcpy, "/", CURPATH_SIZE-1);

    //printf("Advancing to '%s'\n", pathcpy);
    DIR *new = opendir(pathcpy); 
    if (new == NULL) {
        explain("opendir()");
        free(pathcpy);
        return;
    }

    closecurdir(dirp);
    *dirp = new;
    strcpy(curpath, pathcpy);
    strcpy(prevpath, pathcpy);
    free(pathcpy);
}

void
entryhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    
    // Select mode
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_S)) {
        char *dest = clstr2str(elementid.stringId);
        doselect(dest);
        free(dest);
        return;
    }

    // Normal
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    DIR **dirp = (DIR**)userdata;
    char *dest = clstr2str(elementid.stringId);

    if (choosemode) strncpy(chosen, dest, CHOSEN_SIZE);
    else advance(dirp, dest);

    free(dest);
}

void
goback(DIR **dirp) {
    // Nowhere to go back to
    if (strlen(curpath) == 1) return;

    closecurdir(dirp);

    strncpy(prevpath, curpath, CURPATH_SIZE);

    char *p = curpath + strlen(curpath) - 1;
    if (*p == '/') *p = 0;

    for (; *p != '/' && p > curpath; p--) {}
    *(p+1) = 0;

    opencurdir(dirp);

    strncpy(message, "Navigated BACK", MESSAGE_SIZE);
}

void
redo(DIR **dirp) {
    if (!strcmp(curpath, prevpath)) return;

    closecurdir(dirp);

    strcpy(curpath, prevpath);

    opencurdir(dirp);

    strncpy(message, "REDO", MESSAGE_SIZE);
}


void
backhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    DIR **dirp = (DIR**)userdata;

    goback(dirp);
}

void
gohome(DIR **dirp) {
    closecurdir(dirp);

    struct passwd *pw = getpwuid(getuid());

    strncpy(curpath, pw->pw_dir, CURPATH_SIZE-1);
    strncat(curpath, "/", CURPATH_SIZE-1);

    opencurdir(dirp);
}

void
homehandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    DIR **dirp = (DIR**)userdata;
    gohome(dirp);
}

void
togglehide(void) {

    if (hidemode)
        strncpy(message, "Hide mode OFF", MESSAGE_SIZE-1);
    else
        strncpy(message, "Hide mode ON", MESSAGE_SIZE-1);

    hidemode = !hidemode;
}

void
hidehandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    togglehide();
}

Clay_Color
getclr(unsigned char type) {
    switch (type) {
        case DT_DIR:
        return (Clay_Color) CLR_TXT_DIR;
        case DT_LNK:
        return (Clay_Color) CLR_TXT_LNK;
        case DT_BLK:
        return (Clay_Color) CLR_TXT_BLK;
        case DT_SOCK:
        return (Clay_Color) CLR_TXT_SCK;
        case DT_FIFO:
        return (Clay_Color) CLR_TXT_PIP;
        case DT_CHR:
        return (Clay_Color) CLR_TXT_CHR;
    }
    return (Clay_Color) CLR_TXT;
}

void
fileentry(DIR **dirp, const char *str, unsigned char type, int ord) {
    if (!strcmp(str, ".")) return;
    if (!strcmp(str, "..")) return;

    if (hidemode && *str == '.') return;
    //printf("FE: '%s'\n", str);

    Clay_Color clr = getclr(type);

    strcpy(estrs[ord], str);
    
    CLAY({.id = CLAY_SID(((Clay_String){.length = strlen(str), .chars = estrs[ord]})),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(FONT_SIZE+2, FONT_SIZE+10)},
          //.padding = Clay_Hovered() ? 48 : 16,
          .padding = 16,
          .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
          },
            //.backgroundColor = CLR_HOVER(CLR_BG_HOVER, CLR_BG)
            .backgroundColor = CLR_BG,
            //.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_HOVER(CLR_SELECT, CLR_TXT)}
          }) {

        Clay_OnHover(entryhandle, (intptr_t)dirp);
        CLAY_TEXT(
            ((Clay_String){.length = strlen(str), .chars = estrs[ord]}),
            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE,
                .textColor = (Clay_Hovered()||isselect(str)) ? (Clay_Color)CLR_SELECT: clr
                //.textColor = clr
            })
        );
    }
}

void
fileentries(DIR **dirp) {
    CLAY({//.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT},
            .id = CLAY_ID("FileListing"),
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                //.padding = {64,0,0,0},
            //.childGap = 16
            },
            .clip = {
                .vertical = true,
                .childOffset = Clay_GetScrollOffset()
            },
         .backgroundColor = (Clay_Color)CLR_BG}) {

        rewinddir(*dirp);
        
        errno = 0;
        struct dirent *dire = readdir(*dirp);
        int i = 0;
        while (dire != NULL) {
            fileentry(dirp, dire->d_name, dire->d_type, i);
            dire = readdir(*dirp);
            i++;
        }
        numfiles = i - 2;

        if (errno) {
            perror("readdir()");
            exit(1);
        }

    }
}

void
makedir(const char *name) {
    char *path = malloc(strlen(curpath) + strlen(name) + 1);
    strcpy(path, curpath);
    strcat(path, name);

    if (mkdir(path, 0755)) explain("mkdir()");

    free(path);
}

void
delselect(void) {
    for (size_t i = 0; i < selected_sz; i++) {
        if (remove(selected[i])) {
            //perror("remove()");
            //snprintf(message, MESSAGE_SIZE, "%s: %s", selected[i], strerror(errno));
            explain("remove()");
        }
    }
}

void
listselect(void) {
    for (size_t i = 0; i < selected_sz; i++) {
        CLAY_TEXT(((Clay_String){.chars = selected[i], .length = strlen(selected[i])}),
                CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT}));
    }
}

char *
namepart(const char *path) {
    size_t len = strlen(path);
    if (len <= 1) return (char*)path;

    char *p = (char*)path + len;

    if (*p == '/') p--;

    for (; *p != '/'; p--);

    return p + 1;
}

void
move(const char *path) {
    char *namep = namepart(path);
    char *newpath = malloc(strlen(curpath) + strlen(namep) + 1);
    strcpy(newpath, curpath);
    strcat(newpath, namep);

    rename(path, newpath);
    
    free(newpath);
}

void
movesel(void) {
    if (!selected_sz) return;

    for (size_t i = 0; i < selected_sz; i++) {
        move(selected[i]);
    }
}

void copy(const char*, const char*);

void
copys(const char *fromdir, const char *todir) {
    DIR *dir = opendir(fromdir);
    if (dir == NULL) {
        explain("opendir()");
        return;
    }

    errno = 0;
    struct dirent *dire = readdir(dir);

    char *filepath = malloc(strlen(fromdir) + 2);
    strcpy(stpcpy(filepath, fromdir), "/");

    while (dire != NULL) {
        if (!strcmp(dire->d_name, ".") || !(strcmp(dire->d_name, ".."))) {
            dire = readdir(dir);
            continue;
        }

        filepath = realloc(filepath, strlen(dire->d_name) + strlen(fromdir) + 2);
        strcpy(filepath + strlen(fromdir) + 1, dire->d_name);
        copy(filepath, todir);
        dire = readdir(dir);
    }

    if (errno) explain("readdir()");

    free(filepath);
    closedir(dir);
}

void
copydir(const char *path, const char *to) {
    char *namep = namepart(path);

    char *newpath = malloc(strlen(to) + strlen(namep) + 2);
    strcpy(newpath, to);
    strcat(newpath, namep);

    if(mkdir(newpath, 0755)) {
        explain("mkdir()");
        free(newpath);
        return;
    }

    // TODO: check that there is no '/' in namepart
    strcpy(newpath + strlen(to) + strlen(namep), "/");

    copys(path, newpath);

    free(newpath);
}

void
copyfile(const char *path, const char *todir) {
    char *namep = namepart(path);
    char *newpath = malloc(strlen(todir) + strlen(namep) + 2);

    strcpy(newpath, todir);
    strcat(newpath, "/");
    strcat(newpath, namep);

    // TODO: use fstat() to copy file permissions too

    int from = open(path, O_RDONLY);
    int to = creat(newpath, 0644);

    if (from < 0) {
        perror("open()");
        snprintf(message, MESSAGE_SIZE, "%s: %s", path, strerror(errno));
        if (to >= 0) close(to);
        return;
    }

    if (to < 0) {
        perror("open()");
        snprintf(message, MESSAGE_SIZE, "%s: %s", path, strerror(errno));
        close(from);
        return;
    }

    const off_t size = lseek(from, 0, SEEK_END);
    lseek(from, 0, SEEK_SET);

    // Copying done by kernel
    sendfile(to, from, NULL, size);
}


void
copy(const char *path, const char *to) {
    struct stat buf;
    
    if (stat(path, &buf)) {
        explain("stat()");
        return;
    }
    
    if (buf.st_mode & S_IFDIR)
        copydir(path, to);
    else
        copyfile(path, to);
}

void
paste(void) {
    if (!selected_sz) return;

    for (size_t i = 0; i < selected_sz; i++) {
        copy(selected[i], curpath);
    }
}


int
main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INIT_SCRW, INIT_SCRH, "App Window");
    SetTargetFPS(60);
    SetExitKey(0);

    // I'm not sure if 4800 is a correct number, but cyrillic
    // doesn't work with smaller numbers. I guess this is the amount 
    // of glyphs to load, and default (0) doesn't load
    // cyrillics.
    Font font = LoadFontEx("CascadiaCode.ttf", FONT_SIZE, NULL, 4800);
    //SetTextureFilter(font.texture, /*TEXTURE_FILTER_BILINEAR*/TEXTURE_FILTER_POINT);
    //SetTextLineSpacing(16);

    const size_t memory = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memory, malloc(memory));
    Clay_Initialize(arena, (Clay_Dimensions){INIT_SCRW, INIT_SCRH}, (Clay_ErrorHandler){errorhandle});
    Clay_SetMeasureTextFunction(measuretext, &font);

    struct passwd *pw = getpwuid(getuid());

    strncpy(curpath, pw->pw_dir, CURPATH_SIZE);
    strncat(curpath, "/", CURPATH_SIZE-1);

    strcpy(message, "Welcome!");

    DIR *dir = opendir(curpath);
    if (dir == NULL) {
        perror("opendir()");
        exit(1);
    }

    char inputbuf[256] = {0};
    bool make_dir = false;
    bool input_mode = false;
    bool del_mode = false;
    bool del_select_mode = false;

    while (!WindowShouldClose()) {
        Clay_SetLayoutDimensions((Clay_Dimensions){GetScreenWidth(), GetScreenHeight()});
        Clay_SetPointerState((Clay_Vector2){GetMousePosition().x, GetMousePosition().y}, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        Clay_UpdateScrollContainers(false, (Clay_Vector2){0, GetMouseWheelMove()*MOUSE_SCROLL_SENSITIVITY}, GetFrameTime());

        Clay_BeginLayout();

        if (input_mode) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (strlen(inputbuf) < 255)) {
                    const int ln = strlen(inputbuf);
                    inputbuf[ln] = key;
                    inputbuf[ln+1] = 0;
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (strlen(inputbuf))
                    inputbuf[strlen(inputbuf)-1] = 0;
            }

            if (IsKeyPressed(K_CANCEL)) {
                input_mode = false;
                memset(inputbuf, 0, 256);
            }
            
            if (IsKeyPressed(K_ACCEPT_SERIOUS) && strlen(inputbuf)) {
                input_mode = false;
                if (make_dir) {
                    makedir(inputbuf);
                }
                memset(inputbuf, 0, 256);
            }
        }
        else if (del_mode) {
            if (IsKeyPressed(K_CANCEL)) {
                del_mode = false;
                choosemode = false;
                *chosen = 0;
            }

            if ((IsKeyPressed(K_ACCEPT_SERIOUS) || IsKeyPressed(K_ACCEPT))
                    && choosemode && strlen(chosen)) {
                choosemode = false;
                del_mode = false;
                char *filepath = malloc(strlen(curpath) + strlen(chosen) + 1);
                strcpy(filepath, curpath);
                strcat(filepath, chosen);
                if (remove(filepath)) explain("remove()");
                free(filepath);
                *chosen = 0;
            }
        }
        else if (del_select_mode) {
            if (IsKeyPressed(K_ACCEPT_SERIOUS)) {
                del_select_mode = false;
                delselect();
                freeselect();
            }
            else if (IsKeyPressed(K_CANCEL)) {
                del_select_mode = false;
            }
        }
        else {
            if (IsKeyPressed(K_QUIT)) {
                break;
            }
            else if (IsKeyPressed(K_BACK)) {
                goback(&dir);
            }
            else if (IsKeyPressed(K_REDO)) {
                redo(&dir);
            }
            // Go to '~'
            else if (IsKeyPressed(K_HOME)) {
                gohome(&dir);
            }
            else if (IsKeyPressed(K_HIDE)) {
                togglehide();
            }
            // Delete selected, or delete one file
            else if (IsKeyPressed(K_DEL)) {
                if (selected_sz) {
                    del_select_mode = true;
                }
                else {
                    del_mode = true;
                    choosemode = true;
                }
            }
            // Paste
            else if (IsKeyPressed(K_COPY)) {
                paste();
                freeselect();
            }
            // Move 
            else if (IsKeyPressed(K_MOVE)) {
                movesel();
                freeselect();
            }
            // Make directory
            else if (IsKeyPressed(K_MAKEDIR)) {
                input_mode = true;
                make_dir = true;
            }
            // Unselect all
            else if (IsKeyPressed(K_CANCEL)) {
                freeselect();
            }
        }

        char numfiles_str[256];
        snprintf(numfiles_str, 256, "%d", numfiles);

        CLAY({.id = CLAY_ID("OuterContainer"),
              .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
              },
              .backgroundColor = (Clay_Color)CLR_BG}){

            CLAY({.id = CLAY_ID("TopBar"), .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(FONT_SIZE)},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                 }, .backgroundColor = (Clay_Color)CLR_BG_TOPBAR}) {

                CLAY({.layout = {},
                     .border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT_TOPBAR}}) {

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(backhandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("<"), CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(homehandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("~"), CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(hidehandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("."), CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }
                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30*7), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_BG_TOPBAR}) {
                        CLAY_TEXT(((Clay_String){.length = strlen(numfiles_str), .chars = numfiles_str}),
                                CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT_TOPBAR}));
                    }
                }

                CLAY_TEXT(((Clay_String){.length = strlen(curpath), .chars = curpath}), CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT_TOPBAR}));
            }


            fileentries(&dir);

            if (strlen(chosen) && del_mode)
            CLAY({.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT},
                  .floating = {.attachPoints = {.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                               .element = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .attachTo = CLAY_ATTACH_TO_PARENT
                              },
                  .backgroundColor = CLR_BG,
                  .layout = {
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {16,32,16,16},
                 }}) {

                 CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                    CLAY_TEXT(CLAY_STRING("Are you sure you want to remove this file/dir?"),
                            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT}));
                    CLAY_TEXT(((Clay_String){.chars = chosen, .length = strlen(chosen)}),
                            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT_TODEL}));
                 }
            }
            
            //////////////////////////////////////////////////////// Input something (directory name)
            if (input_mode)
            CLAY({.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT},
                  .floating = {.attachPoints = {.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                               .element = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .attachTo = CLAY_ATTACH_TO_PARENT
                              },
                  .backgroundColor = CLR_BG,
                  .layout = {
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {16, 32, 16, 16}
                 }}) {
                
                 CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                    CLAY_TEXT(CLAY_STRING("Create a directory?"),
                            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT}));
                    CLAY_TEXT(((Clay_String){.chars = inputbuf, .length = strlen(inputbuf)}),
                            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT_MAKEDIR}));
                 }
            }

            /////////////////////////////////////////////////////////////// Delete selected entries
            if (del_select_mode)
            CLAY({.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT},
                  .floating = {.attachPoints = {.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                               .element = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .attachTo = CLAY_ATTACH_TO_PARENT
                              },
                  .backgroundColor = CLR_BG,
                  .layout = {
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    .padding = {16, 32, 16, 16}
                 }}) {
                
                 CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                    CLAY_TEXT(CLAY_STRING("Delete following?"),
                            CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT}));

                    listselect();
                    //CLAY_TEXT(((Clay_String){.chars = inputbuf, .length = strlen(inputbuf)}),
                    //        CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT_MAKEDIR}));
                 }
            }

#ifdef BOTTOM_BAR
            CLAY({.id = CLAY_ID("BottomBar"), .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(FONT_SIZE)},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                 }, .backgroundColor = (Clay_Color)CLR_BG, .border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT}}) {
                CLAY_TEXT(((Clay_String){.length = strlen(message), .chars = message}), CLAY_TEXT_CONFIG({.fontSize = FONT_SIZE, .textColor = CLR_TXT}));
            }
#endif
        };

        Clay_RenderCommandArray rendercomm = Clay_EndLayout();

        BeginDrawing();
        ClearBackground((Color)CLR_BG);
        for (int i = 0; i < rendercomm.length; i++) {
            const Clay_RenderCommand *comm = &rendercomm.internalArray[i];
            const Clay_BoundingBox box = comm->boundingBox;

            switch (comm->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                DrawRectangle(box.x, box.y, box.width, box.height, COLOR_CL2R(comm->renderData.rectangle.backgroundColor));
                break;

                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                //color, cornerRaidus, width
                DrawRectangleLinesEx((Rectangle){box.x, box.y, box.width, box.height},
                        comm->renderData.border.width.left,
                        COLOR_CL2R(comm->renderData.border.color));
                break;

                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                char *str = slice2str(comm->renderData.text.stringContents);
                const int16_t fontsz = comm->renderData.text.fontSize;
                const Color clr = COLOR_CL2R(comm->renderData.text.textColor);
                DrawTextEx(font, str, (Vector2){comm->boundingBox.x, comm->boundingBox.y}, fontsz, TEXT_SPACING, clr);
                free(str);
                break;

                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                BeginScissorMode(box.x, box.y, box.width, box.height);
                break;

                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                EndScissorMode();
                break;

                default:
                printf("Unknown render command!\n");
                exit(1);
            }
        }
        EndDrawing();
    }

    CloseWindow();
    freeselect();

    return 0;
}
