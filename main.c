#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

// Getting home directory
#include <unistd.h>
#include <pwd.h>

#include <raylib.h>
#define CLAY_IMPLEMENTATION
#include "clay.h"

#define INIT_SCRW 900
#define INIT_SCRH 600
#define CLR_BG {0, 0, 0, 255}
#define CLR_TXT {255, 255, 255, 255}
#define CLR_TXT_LNK {0, 150, 255, 255}
#define CLR_TXT_DIR {255, 0, 255, 255}
#define CLR_BG_TOPBAR {255, 255, 255, 255}
#define CLR_TXT_TOPBAR {0, 0, 0, 255}
#define CLR_BG_HOVER {255, 255, 255, 255}
#define CLR_TXT_HOVER {0, 0, 0, 255}
#define CLR_BG_TOPBAR_HOVER {0, 0, 0, 255}
#define CLR_TXT_TOPBAR_HOVER {255, 255, 255, 255}

#define MOUSE_SCROLL_SENSITIVITY 5

#define COLOR_CL2R(cc) (Color){.r=cc.r,.g=cc.g,.b=cc.b,.a=cc.a}
#define CLR_HOVER(c1,c2) (Clay_Hovered() ? (Clay_Color) c1 : (Clay_Color) c2)

#define CURPATH_SIZE 1024
#define MESSAGE_SIZE 256

char curpath[CURPATH_SIZE];
char message[MESSAGE_SIZE];
bool hidemode = true;

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
    //// Note: only monospace fonts
    //Clay_Dimensions fuckyourmomcanyouprovideproperfuckinggoddamndocsqqq = (Clay_Dimensions) {
    //        .width = text.length * config->fontSize,
    //        .height = config->fontSize
    //};
    //return fuckyourmomcanyouprovideproperfuckinggoddamndocsqqq;
    const Font *font = (Font*)userData;
    char *str = slice2str(text);
    const Vector2 measure = MeasureTextEx(*font, slice2str(text), config->fontSize, 1);
    free(str);

    return (Clay_Dimensions) {.width = measure.x, .height = measure.y};
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

void backhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata);

void
entryhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    DIR **dirp = (DIR**)userdata;
    char *dest = clstr2str(elementid.stringId);

    char *pathcpy = malloc(CURPATH_SIZE);
    strncpy(pathcpy, curpath, CURPATH_SIZE-1);

    strncat(pathcpy, dest, CURPATH_SIZE-1);
    strncat(pathcpy, "/", CURPATH_SIZE-1);

    free(dest);

    DIR *new = opendir(pathcpy); 
    if (new == NULL) {
        perror("opendir()");
        free(pathcpy);
        return;
    }

    closecurdir(dirp);
    *dirp = new;
    strcpy(curpath, pathcpy);
    free(pathcpy);
}

void
backhandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    // Nowhere to go back to
    if (strlen(curpath) == 1) return;

    DIR **dirp = (DIR**)userdata;
    closecurdir(dirp);

    char *p = curpath + strlen(curpath) - 1;
    if (*p == '/') *p = 0;

    for (; *p != '/' && p > curpath; p--) {}
    *(p+1) = 0;

    opencurdir(dirp);
}

void
homehandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    DIR **dirp = (DIR**)userdata;

    closecurdir(dirp);

    struct passwd *pw = getpwuid(getuid());

    strncpy(curpath, pw->pw_dir, CURPATH_SIZE-1);
    strncat(curpath, "/", CURPATH_SIZE-1);

    opencurdir(dirp);
}

void
hidehandle(Clay_ElementId elementid, Clay_PointerData pointerdat, intptr_t userdata) {
    if (pointerdat.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;

    hidemode = !hidemode;
}

void
fileentry(DIR **dirp, const char *str, unsigned char type) {
    //bool isStaticallyAllocated;
    //int32_t length;
    //// The underlying character memory. Note: this will not be copied and will not extend the lifetime of the underlying memory.
    //const char *chars;
    
    if (!strcmp(str, ".")) return;
    if (!strcmp(str, "..")) return;

    if (hidemode && *str == '.') return;

    Clay_Color clr = CLR_TXT;
    if (type == DT_DIR)
        clr = (Clay_Color) CLR_TXT_DIR;
    if (type == DT_LNK)
        clr = (Clay_Color) CLR_TXT_LNK;

    CLAY({.id = CLAY_SID(((Clay_String){.length = strlen(str), .chars = str})),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0, 50)}, .padding = 16},
          .backgroundColor = CLR_HOVER(CLR_BG_HOVER, CLR_BG)}) {

        Clay_OnHover(entryhandle, (intptr_t)dirp);
        CLAY_TEXT(
            ((Clay_String){.length = strlen(str), .chars = str}),
            CLAY_TEXT_CONFIG({.fontSize = 24, .textColor = CLR_HOVER(CLR_TXT_HOVER, clr)})
        );
    }
}

void
fileentries(DIR **dirp) {
    CLAY({.border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT}, .id = CLAY_ID("FileListing"), .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
            //.padding = CLAY_PADDING_ALL(16),
            //.childGap = 16
         }, .clip = {
            .vertical = true,
            .childOffset = Clay_GetScrollOffset()
         },
         .backgroundColor = (Clay_Color)CLR_BG}) {

        rewinddir(*dirp);
        
        errno = 0;
        struct dirent *dire = readdir(*dirp);

        while (dire != NULL) {
            fileentry(dirp, dire->d_name, dire->d_type);
            dire = readdir(*dirp);
        }

        if (errno) {
            perror("readdir()");
            exit(1);
        }

    }
}

int
main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INIT_SCRW, INIT_SCRH, "App Window");
    SetTargetFPS(60);

    Font font = LoadFontEx("CascadiaCode.ttf", 32, 0, 250);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextLineSpacing(16);

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(Clay_MinMemorySize(), malloc(Clay_MinMemorySize()));
    Clay_Initialize(arena, (Clay_Dimensions){INIT_SCRW, INIT_SCRH}, (Clay_ErrorHandler){errorhandle});
    Clay_SetMeasureTextFunction(measuretext, &font);

    strcpy(curpath, "/home/w/Programming/");
    strcpy(message, "Welcome!");

    DIR *dir = opendir(curpath);
    if (dir == NULL) {
        perror("opendir()");
        exit(1);
    }

    while (!WindowShouldClose()) {
        Clay_SetLayoutDimensions((Clay_Dimensions){GetScreenWidth(), GetScreenHeight()});
        Clay_SetPointerState((Clay_Vector2){GetMousePosition().x, GetMousePosition().y}, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        Clay_UpdateScrollContainers(false, (Clay_Vector2){0, GetMouseWheelMove()*MOUSE_SCROLL_SENSITIVITY}, GetFrameTime());

        Clay_BeginLayout();

        CLAY({.id = CLAY_ID("OuterContainer"),
              .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}
              },
              .backgroundColor = (Clay_Color)CLR_BG}){

            CLAY({.id = CLAY_ID("TopBar"), .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(24)},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                 }, .backgroundColor = (Clay_Color)CLR_BG_TOPBAR}) {

                CLAY({.layout = {},
                     .border = {.width = CLAY_BORDER_ALL(1), .color=CLR_TXT_TOPBAR}}) {

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(backhandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("<"), CLAY_TEXT_CONFIG({.fontSize = 24,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(homehandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("~"), CLAY_TEXT_CONFIG({.fontSize = 24,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }

                    CLAY({.layout = {
                            .sizing = {CLAY_SIZING_GROW(0, 30), CLAY_SIZING_GROW(0)},
                            .padding = {8, 8, 0, 0}
                         }, .backgroundColor = CLR_HOVER(CLR_BG_TOPBAR_HOVER, CLR_BG_TOPBAR)}) {
                        Clay_OnHover(hidehandle, (intptr_t)&dir);
                        CLAY_TEXT(CLAY_STRING("."), CLAY_TEXT_CONFIG({.fontSize = 24,
                                    .textColor = CLR_HOVER(CLR_TXT_TOPBAR_HOVER, CLR_TXT_TOPBAR)}));
                    }
                }

                CLAY_TEXT(((Clay_String){.length = strlen(curpath), .chars = curpath}), CLAY_TEXT_CONFIG({.fontSize = 24, .textColor = CLR_TXT_TOPBAR}));
            }


            fileentries(&dir);

            CLAY({.id = CLAY_ID("BottomBar"), .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(24)},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                 }, .backgroundColor = (Clay_Color)CLR_BG}) {
                CLAY_TEXT(((Clay_String){.length = strlen(curpath), .chars = message}), CLAY_TEXT_CONFIG({.fontSize = 24, .textColor = CLR_TXT}));
            }
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
                DrawTextEx(font, str, (Vector2){comm->boundingBox.x, comm->boundingBox.y}, fontsz, 2, clr);
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
}
