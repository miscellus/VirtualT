/* m100emu.c */

/* $Id: m100emu.c,v 1.41 2013/03/08 00:41:16 kpettit1 Exp $ */

/*
 * Copyright 2004 Stephen Hurd and Ken Pettit
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


// --------------------------------------------
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>

#ifdef __unix__
#include <signal.h>
#include <unistd.h>
#endif

#include "VirtualT.h"
#include "io.h"
#include "cpu.h"
#include "doins.h"
#include "display.h"
#include "setup.h"
#include "memory.h"
#include "m100emu.h"
#include "sound.h"

// int              gModel = MODEL_M100;
// int              gModel = MODEL_M10;
int             gModel = MODEL_PC8201;
// int              gModel = MODEL_PC8300;

volatile uchar  cpu[14];
extern uchar    gOptROM[32768];

char                    op[26];
int                     fullspeed = 0;
volatile UINT64         cycles=0;
volatile int            cycle_delta;

double                  last_instruct = 0;
static volatile UINT64  last_isr_cycle = 0;
int                     trace=0;
int                     starttime;
FILE                    *tracefile;
volatile DWORD          rst7cycles = 9830;
static time_t           gLptTime;
static UINT64           one_sec_cycles;
static time_t           one_sec_time;
float                   cpu_speed;
float                   gCpuSpeedAvg[4];
int                     gCpuSpeedAvgIndex = 0;
volatile int            gExitApp = 0;
volatile int            gExitLoop = 0;
char                    gsOptRomFile[256];
int                     gShowVersion = 0;
int                     gMaintCount = 65536;
int                     gOsDelay = 0;
int                     gNoGUI = 0;
int                     gRemoteSwitchModel = -1;

//Added J. VERNET
char                    path[512];
char                    file[512];
#ifdef __APPLE__
char                    gOsxBundlePath[512];
#endif

extern RomDescription_t     gM100_Desc;
extern RomDescription_t     gM200_Desc;
extern RomDescription_t     gN8201_Desc;
extern RomDescription_t     gM10_Desc;
extern RomDescription_t     gKC85_Desc;


extern uchar                gReMem;
extern int                  cROM;
extern unsigned char        ioD0;
RomDescription_t            *gStdRomDesc = NULL;


/* Define Debug global variables */
char                    gDebugInts = TRUE;
char                    gIntActive = 0;
unsigned short          gIntSP = 0;

static void *gCoFileContents;
static size_t gCoFileLength;


void show_error(const char *message)
{
    fprintf(stderr, "[ERROR] %s\n", message);
}

/*
=============================================================================
This routine bails from the app in case of a panic.
=============================================================================
*/
void bail(char *msg)
{
    double endtime=SDL_GetTicksNS()/1000000.0;
    puts(msg);
    printf("%-22s A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X PC:%04X SP:%04X IM:%02X *SP:%04X\n",op,A,F,B,C,D,E,H,L,PC,SP,IM,MEM16(SP));
    printf("Start: %d  End: %.2f\n",starttime,endtime);
    printf("Time: %.2f\n",((double)(endtime-starttime))/1000);
    printf("MHz: %f\n",((UINT64)cycles/(((double)(endtime-starttime))/1000))/1000000);

    exit(1);
}

/*
=============================================================================
This routine performs a quick "warm" reset with no re-initialization of mem.
=============================================================================
*/
void zero_cpu_registers(void)
{
    /* Clear all CPU registers */
    for (int i = 0; i < sizeof(cpu); ++i)
        cpu[i] = 0;
}

/*
=============================================================================
get_model_string:   This function returns the sring name of the specified
                    emulation model.
=============================================================================
*/
void get_model_string(char* str, int model)
{
    switch (model)
    {
    case MODEL_M100:
        strcpy(str, "m100");
        break;
    case MODEL_M102:
        strcpy(str, "m102");
        break;
    case MODEL_T200:
        strcpy(str, "t200");
        break;
    case MODEL_M10:
        strcpy(str, "m10");
        break;
    case MODEL_PC8201:
        strcpy(str, "pc8201");
        break;
    case MODEL_KC85:
        strcpy(str, "kc85");
        break;
    case MODEL_PC8300:
        strcpy(str, "pc8300");
        break;
    }
}

/*
=============================================================================
get_model_from_string:  This function returns the model number given the
                        sring name specified.
=============================================================================
*/
int get_model_from_string(char* str)
{
    if (strcmp("m100", str) == 0)
        return MODEL_M100;
    else if (strcmp("m102", str) == 0)
        return MODEL_M102;
    else if (strcmp("t200", str) == 0)
        return MODEL_T200;
    else if (strcmp("pc8201", str) == 0)
        return MODEL_PC8201;
    else if (strcmp("pc8300", str) == 0)
        return MODEL_PC8300;
    else if (strcmp("m10", str) == 0)
       return MODEL_M10;
    else if (strcmp("kc85", str) == 0)
        return MODEL_KC85;

    return -1;
}

/*
=============================================================================
get_emulation_path: This function returns the path of the emulation directory
                    based on the supplied model.
=============================================================================
*/
void get_emulation_path(char* emu, int model)
{
    strcpy(emu, path);          /* Copy VirtualT path */
    switch (model)
    {
    case MODEL_M100:
        strcat(emu, "M100/");
        break;
    case MODEL_M102:
        strcat(emu, "M102/");
        break;
    case MODEL_T200:
        strcat(emu, "T200/");
        break;
    case MODEL_M10:
        strcat(emu, "M10/");
        break;
    case MODEL_PC8201:
        strcat(emu, "PC8201/");
        break;
    case MODEL_KC85:
        strcat(emu, "KC85/");
        break;
    case MODEL_PC8300:
        strcat(emu, "PC8300/");
        break;
    }
}

/*
=============================================================================
get_rom_path:   This function returns the path of the ROM file for the
                model specified.  The path is strcpy(ed) into the string
                supplied.
=============================================================================
*/
void get_rom_path(char* file, int model)
{
    strcpy(file, path);         /* Copy VirtualT path */

    switch (model)
    {
    case MODEL_M100:
        strcat(file, "M100/M100rom.bin");
        break;
    case MODEL_M102:
        strcat(file, "M102/M102rom.bin");
        break;
    case MODEL_T200:
        strcat(file, "T200/T200rom.bin");
        break;
    case MODEL_M10:
        strcat(file, "M10/M10rom.bin");
        break;
    case MODEL_PC8201:
        strcat(file, "PC8201/PC8201rom.bin");
        break;
    case MODEL_KC85:
        strcat(file, "KC85/KC85rom.bin");
        break;
    case MODEL_PC8300:
        strcat(file, "PC8300/PC8300rom.bin");
        break;
    }
}

/*
=============================================================================
check_model_support:    This function checks for support for the specified
                        Model and returns TRUE if that model is supported.

                        Model Support is determined by checking for the
                        appropriate directory structure with the correct ROM
                        file.
=============================================================================
*/
int check_model_support(int model)
{
    char    file[256];
    FILE*   fd;

    /* Get the path for the model supplied */
    get_rom_path(file, model);

    /* Attempt to open the ROM file */
    if ((fd = fopen(file, "r")) == NULL)
        return FALSE;

    /* Open successful, close the file and return */
    fclose(fd);
    return TRUE;
}

/*
=============================================================================
check_installation: This routine checks that VirtualT is properly installed
                    with model directories and appropriate rom files.

                    If the files are not installed, it attempts to create
                    them using files in the ROMs directory.
=============================================================================
*/
void check_installation(void)
{
    int     model;
    char    localpath[256];
    char    roms_path[512];
    char    errors[256];

    /* Test if Mac OSX and no path specified */
#ifdef __APPLE__
    struct stat romStat;
    if (strlen(path) == 0)
        return;
#endif

    errors[0] = 0;

    /* Check each model */
    for (model = MODEL_M100; model <= MODEL_PC8300; model++)
    {
        /* Check if ROM file exists for this model */
        if (check_model_support(model))
            continue;

        /* ROM file doesn't exist.  Try to open in ROMs dir */
        get_rom_path(localpath, model);
#if defined(__APPLE__)
        sprintf(roms_path, "%sROMs%s", path, strrchr(localpath, '/'));

        /* Test if the ROM file exists in the working directory */
        /* stat() is replaced by SDL_GetPathInfo */
        if (!SDL_GetPathInfo(roms_path, NULL))
        {
            /* Test if running from a bundle & get the bundle path */
            if (strlen(gOsxBundlePath) > 0)
            {
                sprintf(roms_path, "%s/Resources%s", gOsxBundlePath, strrchr(localpath, '/'));
            }
        }
#else
        sprintf(roms_path, "ROMs%s", strrchr(localpath, '/'));
#endif

        /* Replaces the old fopen(roms_path, "rb") == NULL check */
        if (!SDL_GetPathInfo(roms_path, NULL))
        {
            /* Error - ROM file not in ROMs dir */
            if (strlen(errors) != 0)
                strcat(errors, ", ");
            get_model_string(localpath, model);
            strcat(errors, localpath);
            continue;
        }

        /* Create the emulation directory */
        get_emulation_path(localpath, model);
        SDL_CreateDirectory(localpath);

        /* Copy the ROM file from ROMs dir using SDL3 */
        get_rom_path(localpath, model);

        if (!SDL_CopyFile(roms_path, localpath))
        {
            if (strlen(errors) != 0)
                strcat(errors, ", ");
            get_model_string(localpath, model);
            strcat(errors, localpath);
            continue;
        }
    }

    if (strlen(errors) > 0)
    {
        sprintf(localpath, "No ROM file for %s", errors);
        show_error(localpath);
    }
}

/*
======================================================================
initcpu:    This function initializes the CPU and the system memories
            including loading the ROM and RAM files.
======================================================================
*/
void init_cpu(void)
{
    int     i;

    /* Initialize CPU registers */
    A = F = B = C = D = E = H = L = 0;
    SPH = SPL = 0;
    PCH = PCL = 0;
    IM=0x08;
    cpuMISC=0;

    load_sys_rom();

    /* Clear the system memory RAM area */
    for (i = 0; i < RAMSIZE; i++)
        gBaseMemory[RAMSTART + i] = 0;

    /* Read RAM from file in emulation directory */
    //load_ram();

    /* Load option ROM if any */
    load_opt_rom();

    gExitLoop = 1;
}


/*
========================================================================
This routine peforms a CPU reset and reinitializes memory.
========================================================================
*/
void resetcpu(void)
{
    zero_cpu_registers();

    /* Set interrupt mask */
    IM=0x08;
    cpuMISC=0;

    /* Re-initialize memory */
    reinit_mem();
}

void cb_int65(int pinLevel)
{
    /* The INT6.5 pin is level sensitive.  Set the interrupt pending bit based on current level */
    if (pinLevel)
    {
        /* Set the INT6.5 pending bit high */
        IM|=0x20;
        if(trace && tracefile != NULL)
            fprintf(tracefile,"RST 6.5 Issued\n");
    }
    else
    {
        /* Clear the INT6.5 pending bit */
        IM &= 0xDF;
    }
}

/*
========================================================================
This routine processes CPU interrupts.
========================================================================
*/
Uint64 check_interrupts(void)
{
    static volatile UINT64  last_rst75=0;

    Uint64 cycle_delta = 0;

    if (((last_rst75 + rst7cycles) < cycles) && !INTDIS)
    {
        IM |= 0x40;
        if(trace && tracefile != NULL)
            fprintf(tracefile,"RST 7.5 Issued diff = %d\n", (DWORD) (cycles - last_rst75));
        last_rst75=cycles;
    }

    /* TRAP should be first */

    if(RST75PEND && !INTDIS && !RST75MASK)
    {
        if(trace && tracefile != NULL)
            fprintf(tracefile,"RST 7.5 CALLed\n");

        if (gDebugInts)
            gIntActive = TRUE;
        gIntSP = SP;
        DECSP2;
        if (gReMem)
        {
            MEMSET(SP, PCL);
            MEMSET(SP+1, PCH);
        }
        else
        {
            gBaseMemory[SP] = PCL;
            gBaseMemory[SP+1] = PCH;
        }
        /* MEM16(SP)=PC; */
        PCL=60;
        PCH=0;
        /* PC=60; */
        cycle_delta += 10;  /* This may not be correct */
        IM=IM&0xBF;
        last_isr_cycle = cycles;

        if (gDelayUpdateKeys == 1)
            update_keys();
    }
    else if(RST65PEND && !INTDIS && !RST65MASK)
    {
        if(trace && tracefile != NULL)
            fprintf(tracefile,"RST 6.5 CALLed\n");

        if (gDebugInts)
            gIntActive = TRUE;
        gIntSP = SP;
        DECSP2;
        if (gReMem)
        {
            MEMSET(SP, PCL);
            MEMSET(SP+1, PCH);
        }
        else
        {
            gBaseMemory[SP] = PCL;
            gBaseMemory[SP+1] = PCH;
        }

        /* MEM16(SP)=PC; */
        PCL=52;
        PCH=0;
        /* PC=52; */
        cycle_delta += 10;  /* This may not be correct */
//      IM=IM&0xDF;
        last_isr_cycle = cycles;
    }
    return cycle_delta;
}

void remote_switch_model(int model)
{
    gRemoteSwitchModel = model;
}

/* Instruction emulation contained in header file */
#undef NO_REMEM
#include "cpu.h"
#include "do_instruct.h"

static struct do_instruct_state do_instruct_state;

const Sint64 CPU_HZ = 2457600;
const double CYCLES_PER_NS = 2457600.0/1000000000.0;
const Sint64 CYCLES_PER_RST_7_5 = 9830;
const Sint64 DELTA_TIME_MAX_NS = 100*1000*1000;

void emulate_n_cycles(Sint64 n_cycles)
{
    static Sint64 cycle_accumulator = 0;

    if (n_cycles <= 0) return;

    n_cycles *= 4;

    Sint64 cycle_delta = 0;

    while (cycle_delta < n_cycles)
    {
        cycle_delta += do_instruct(&do_instruct_state);

        // Check if next inst is SIM
        if ((get_memory8(PC) == 0xF3) || ((IM & 0x2A) == 0x20) || ((IM & 0x4C) == 0x40))
        {
            cycle_delta += check_interrupts();
        }

        // Check for return from interrupt
        if (gIntActive)
        {
            if (SP == gIntSP)
                gIntActive = FALSE;
        }

        for (
            cycle_accumulator += cycle_delta;
            cycle_accumulator > CYCLES_PER_RST_7_5;
            cycle_accumulator -= CYCLES_PER_RST_7_5)
        {
            cycle_delta += check_interrupts();
        }
    }

    cycles += cycle_delta;
}

/*
========================================================================
handle_sig: This routine handles unix signals and kills the app if there
            is a panic.
========================================================================
*/
#ifdef __unix__
void handle_sig(int sig)
{
    bail("Kill by signal");
}
#endif

/*
========================================================================
process_args:   This routine processes the command-line arguments.
========================================================================
*/
int process_args(int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++)
    {
        // Seach for trace flag
        if (!strcmp(argv[i], "-t"))
        {
            trace = 1;              /* Turn on trace mode */
            tracefile = fopen("trace.txt", "w+");
        }

        // Search for No-GUI flag
        if (!strcmp(argv[i], "-nogui"))
            gNoGUI = 1;

        // Search for Socket Port flag
        if (!strcmp(argv[i], "-run-co-file"))
        {
            // Path to CO-file is next argument
            if (i + 1 >= argc)
            {
                printf("%s: -run-co-file path to CO-file not specified\n", argv[0]);
                return 1;
            }
            else
            {
                gCoFileContents = SDL_LoadFile(argv[i+1], &gCoFileLength);
                i++;
            }
        }

//      if (!strcmp(argv[i], "-t"))
//          set_remote_cmdline_telnet(TRUE);

//      // Search for Socket Port flag
//      if (!strncmp(argv[i], "-p", 2))
//      {
//          // Check if port number is part of flag
//          if (strlen(argv[i]) != 2)
//          {
//              set_remote_cmdline_port(atoi(&argv[i][2]));
//          }
//          else
//          {
//              // Socket number is next argument
//              if (i + 1 >= argc)
//              {
//                  printf("%s: -p port not specified\n", argv[0]);
//                  return 1;
//              }
//              else
//              {
//                  set_remote_cmdline_port(atoi(argv[i+1]));
//                  i++;
//              }
//          }
//      }
    }

    return 0;
}

/*
========================================================================
This routine sets unix signals and links them to a 'bail' routine.
========================================================================
*/
void setup_unix_signals(void)
{
#ifdef __unix__
    signal(SIGTERM,handle_sig);
    signal(SIGHUP,handle_sig);
    signal(SIGQUIT,handle_sig);
    signal(SIGINT,handle_sig);
#endif
}

/*
========================================================================
This routine sets the working path using SDL3's SDL_GetBasePath().
========================================================================
*/
void setup_working_path(char **argv)
{
    // FIX 1: SDL3 returns a const char* that you DO NOT free.
    const char *base_path = SDL_GetBasePath();

    if (base_path)
    {
        // Copy to your global 'path' variable safely
        SDL_strlcpy(path, base_path, sizeof(path));

        // FIX 2: Only compile the bundle path extraction on macOS,
        // otherwise Windows MSVC will complain that gOsxBundlePath is undeclared.
#if defined(__APPLE__)
        const char *pContents = SDL_strstr(base_path, "/Contents/");
        if (pContents)
        {
            // Calculate length up to the end of "/Contents" (9 characters)
            size_t bundle_len = (pContents - base_path) + 9;
            SDL_strlcpy(gOsxBundlePath, base_path, bundle_len + 1);
        }
#endif
    }
    else
    {
        // Fallback if SDL couldn't determine the path
        SDL_Log("Failed to get base path: %s", SDL_GetError());
        path[0] = '\0';
    }
}

////////////////////////////////////////////////////////
// STUBS


/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *lcd_texture = NULL;

#define LCD_WIDTH 240
#define LCD_HEIGHT 64
#define LCD_RENDER_BPP 3
#define LCD_PITCH (LCD_WIDTH * LCD_RENDER_BPP)

static uchar backing_lcd[10][256];
static uchar backing_lcd_top_row[10];

static uchar backing_lcd_pixels[LCD_WIDTH*LCD_HEIGHT*LCD_RENDER_BPP];

#define COLOR_BG 0xadb9a3
#define COLOR_PIXEL 0x3b4c48

static inline drawpixel(int x, int y, int color)
{
    backing_lcd_pixels[(x + y*LCD_WIDTH)*LCD_RENDER_BPP + 0] = ((color ? COLOR_PIXEL : COLOR_BG)>>16) & 0xff;
    backing_lcd_pixels[(x + y*LCD_WIDTH)*LCD_RENDER_BPP + 1] = ((color ? COLOR_PIXEL : COLOR_BG)>>8)  & 0xff;
    backing_lcd_pixels[(x + y*LCD_WIDTH)*LCD_RENDER_BPP + 2] = ((color ? COLOR_PIXEL : COLOR_BG)>>0)  & 0xff;
}

static inline refresh_texture(void)
{
    SDL_UpdateTexture(lcd_texture, NULL, backing_lcd_pixels, LCD_WIDTH * LCD_RENDER_BPP);
}


/*
=================================================================
draw:   This routine draws the entire LCD.  This is a member
        function of Fl_Window.
=================================================================
*/
void draw_entire_lcd_to_texture(void)
{
    int x=0;
    int y=0;
    int driver, col, row;
    uchar value;

    for (driver = 0; driver < 10; driver++)
    {
        for (row = 0; row < 4; row++)
        {
            for (col = 0; col < 50; col++)
            {
                if (((driver == 4) || (driver == 9)) && ((col & 0x3F)  >= 40))
                    continue;

                x=(driver % 5) * 50;
                x+=col;

                y = ((row - backing_lcd_top_row[driver]+4) % 4) << 3;
                if (driver > 4)
                    y += 32;

                if (x > 240)
                    x = x + 1;
                value = backing_lcd[driver][row*64+col];

                // Draw the black pixels
                drawpixel(x,y++, value&0x01);
                drawpixel(x,y++, value&0x02);
                drawpixel(x,y++, value&0x04);
                drawpixel(x,y++, value&0x08);
                drawpixel(x,y++, value&0x10);
                drawpixel(x,y++, value&0x20);
                drawpixel(x,y++, value&0x40);
                drawpixel(x,y++, value&0x80);
            }
        }
    }
    refresh_texture();
}


void run_co_file(void)
{
    if (!gCoFileContents) return;

    ushort *at16 = gCoFileContents;

    if (gCoFileLength <= 6) return;

    ushort load_addr = *at16++;
    ushort program_length = *at16++;
    ushort entry_addr = *at16++;

    unsigned char *at8 = (unsigned char *)(void *)at16;

    if (gCoFileLength < 6 + program_length) return;

    for (ushort addr = load_addr; addr < load_addr+program_length; ++addr)
    {
        set_memory8(addr, *at8++);
    }

    gIntSP = SP;
    DECSP2;
    if (gReMem)
    {
        MEMSET(SP, PCL);
        MEMSET(SP+1, PCH);
    }
    else
    {
        gBaseMemory[SP] = PCL;
        gBaseMemory[SP+1] = PCH;
    }
    /* MEM16(SP)=PC; */
    PCL = 0xff & entry_addr;
    PCH = 0xff & (entry_addr >> 8);
}

int     sound_enable;               // Set TRUE when sound is enabled

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (process_args(argc, argv))   /* Parse command line args */
    {
        SDL_Log("Couldn't parse command line args.");
        return SDL_APP_FAILURE;
    }

    setup_working_path(argv);   /* Create a working dir path */
    setup_unix_signals();       /* Setup Unix signal handling */

    check_installation();       /* Test if install needs to be performed */

    init_mem();                 /* Initialize Memory */
    init_io();                  /* Initialize I/O structures */
    sound_enable = 1;
    init_sound();               /* Initialize Sound system */
    init_cpu();                 /* Initialize the CPU */

    SDL_SetAppMetadata("Example Renderer Clear", "1.0", "com.example.renderer-clear");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Slappy", 240*6, 64*6, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, LCD_WIDTH, LCD_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    lcd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, LCD_WIDTH, LCD_HEIGHT);
    SDL_SetTextureScaleMode(lcd_texture, SDL_SCALEMODE_NEAREST);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

static unsigned long map_special_key(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE:    return MT_ESC;
        case SDL_SCANCODE_BACKSPACE: return MT_BKSP;
        case SDL_SCANCODE_TAB:       return MT_TAB;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:  return MT_ENTER;
        case SDL_SCANCODE_PAUSE:     return MT_PAUSE;
        case SDL_SCANCODE_SPACE:     return MT_SPACE;
        case SDL_SCANCODE_CAPSLOCK:  return MT_CAP_LOCK;

        // Directional
        case SDL_SCANCODE_LEFT:      return MT_LEFT;
        case SDL_SCANCODE_RIGHT:     return MT_RIGHT;
        case SDL_SCANCODE_UP:        return MT_UP;
        case SDL_SCANCODE_DOWN:      return MT_DOWN;

        // Modifiers
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:    return MT_SHIFT;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:     return MT_CTRL;
        case SDL_SCANCODE_LALT:      return MT_GRAPH; // Left Alt maps to GRAPH
        case SDL_SCANCODE_RALT:      return MT_CODE;  // Right Alt maps to CODE

        // Function Keys
        case SDL_SCANCODE_F1:        return MT_F1;
        case SDL_SCANCODE_F2:        return MT_F2;
        case SDL_SCANCODE_F3:        return MT_F3;
        case SDL_SCANCODE_F4:        return MT_F4;
        case SDL_SCANCODE_F5:        return MT_F5;
        case SDL_SCANCODE_F6:        return MT_F6;
        case SDL_SCANCODE_F7:        return MT_F7;
        case SDL_SCANCODE_F8:        return MT_F8;
        case SDL_SCANCODE_F9:        return MT_LABEL;
        case SDL_SCANCODE_F10:       return MT_PRINT;
        case SDL_SCANCODE_F11:       return MT_PASTE;

        default:                     return 0; // Not a special key
    }
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        gExitApp = 1;
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    // Handle Keyboard Events
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
    {

        // Ignore OS-level key repeats (the emulator handles its own repetition)
        if (event->key.repeat) return SDL_APP_CONTINUE;

        bool isDown = (event->type == SDL_EVENT_KEY_DOWN);
        SDL_Scancode scancode = event->key.scancode; // Physical key location
        SDL_Keycode keycode = event->key.key;        // Virtual character mapping

        // 1. Try to map to a special key first
        unsigned int specialFlag = map_special_key(scancode);

        if (specialFlag != 0) {
            // Apply active-low logic for gSpecialKeys
            if (isDown) {
                if (specialFlag == MT_CAP_LOCK)
                    gSpecialKeys ^= specialFlag;
                else
                    gSpecialKeys &= ~specialFlag;

            } else if (specialFlag != MT_CAP_LOCK) {
                gSpecialKeys |= specialFlag;
            }
        }
        else {
            // 2. Handle standard ASCII characters
            // In SDL3, keycodes for standard characters match their ASCII values
            if (keycode > 0 && keycode < 128) {
                int asciiKey = keycode & 0x7F;

                // Apply active-high logic for gKeyStates
                gKeyStates[asciiKey] = isDown ? 1 : 0;
            }
        }

        // Tell the emulator core to process the updated keyboard matrix
        update_keys();
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    static Uint64 time_last = 0;
    Sint64 time_now = SDL_GetTicksNS();
    Sint64 time_delta = time_now - time_last;
    time_last = time_now;

    if (time_delta > DELTA_TIME_MAX_NS)
        time_delta = DELTA_TIME_MAX_NS;

    Uint64 n_cycles = (Uint64)(time_delta * CYCLES_PER_NS);
    emulate_n_cycles(n_cycles);

    SDL_RenderClear(renderer);
    // scale to window automatically; use destination NULL to fill
    draw_entire_lcd_to_texture();
    SDL_RenderTexture(renderer, lcd_texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    static int delay_frames_before_co_run = 10;
    if (delay_frames_before_co_run > 0) {
        --delay_frames_before_co_run;
        if (delay_frames_before_co_run == 0)    run_co_file();

    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */

    /* Save RAM contents after emulation */
    save_ram();

    /* Cleanup */
    deinit_io();                /* Deinitialize I/O */
    deinit_sound();             /* Deinitialize sound */
    free_mem();                 /* Free memory used by ReMem and/or Rampac */
}

void drawbyte(int driver, int col, int value)
{
    int x;
    int y;

//  if (driver < 10)
//  {
        // Check if LCD already has the value being requested
        if (backing_lcd[driver][col] == value)
            return;

        //if (driver == 4)
            //printf("%d=%d ", col, value);

        // Load new value into lcd "RAM"
        backing_lcd[driver][col] = value;

        // Check if value is an LCD command
        if ((col&0x3f) > 49)
            return;

        // Calculate X position of byte
        x=(driver % 5) * 50 + (col&0x3F);

        // Calcluate y position of byte
        y = ((((col & 0xC0) >> 6) - backing_lcd_top_row[driver] + 4) % 4) * 8;
        if (driver > 4)
            y += 32;

        // Draw each pixel of byte
        drawpixel(x,y++,value&0x01);
        drawpixel(x,y++,(value&0x02)>>1);
        drawpixel(x,y++,(value&0x04)>>2);
        drawpixel(x,y++,(value&0x08)>>3);
        drawpixel(x,y++,(value&0x10)>>4);
        drawpixel(x,y++,(value&0x20)>>5);
        drawpixel(x,y++,(value&0x40)>>6);
        drawpixel(x,y++,(value&0x80)>>7);
}

void lcdcommand(int driver, int value) // ref: io.obj in function out
{

}

void power_down() { } // ref: io.obj in function out
void display_map_mode(char *str) { } // ref: io.obj in function show_remem_mode
void t200_command(uchar ir, uchar data) { } // ref: io.obj in function out
uchar t200_readport(uchar port) { return 0; } // ref: io.obj in function inport
void send_to_lpt(uchar byte) { } // ref: io.obj in function out
void init_clock() { } // ref: io.obj in function init_io
void rp5c01_write(uchar port, uchar value) { } // ref: io.obj in function out
uchar rp5c01_read(uchar port) { return 0; } // ref: io.obj in function inport
void pd1990ac_chip_cmd(uchar val) { } // ref: io.obj in function out
void pd1990ac_clk_pulse(uchar val) { } // ref: io.obj in function out
void tdock_read() { } // ref: io.obj in function inport
void tdock_write() { } // ref: io.obj in function out
void handle_wheel_keys() { } // ref: io.obj in function update_keys

peripheral_setup_t setup;
memory_setup_t      mem_setup;  // Memory setup options

void clock_serial_out() { } // ref: io.obj in function inport
void enable_tpdd_log_menu(int bEnabled) { } // ref: serial.obj in function ser_init
void tpdd_alloc_context() { } // ref: serial.obj in function ser_init
void tpdd_free_context() { } // ref: serial.obj in function ser_deinit
void tpdd_load_prefs() { } // ref: serial.obj in function ser_init
void tpdd_close_serial() { } // ref: serial.obj in function ser_close_port
void tpdd_open_serial() { } // ref: serial.obj in function ser_open_port
void tpdd_ser_set_baud() { } // ref: serial.obj in function ser_set_baud
void tpdd_ser_get_flags() { } // ref: serial.obj in function ser_get_flags
void tpdd_ser_set_signals() { } // ref: serial.obj in function ser_set_signals
void tpdd_ser_get_signals() { } // ref: serial.obj in function ser_get_signals
void tpdd_ser_read_byte() { } // ref: serial.obj in function ser_read_byte
void tpdd_ser_write_byte() { } // ref: serial.obj in function ser_write_byte
void tpdd_ser_poll() { } // ref: serial.obj in function ser_poll

void ser_init(void) {}
void ser_deinit(void) {}
void ser_set_baud(void) {}
void ser_set_parity(void) {}
void ser_set_bit_size(void) {}
void ser_set_stop_bits(void) {}
void ser_set_callback(void) {}
uchar ser_get_flags(void) { return 0; }
void ser_set_signals(void) {}
uchar ser_read_byte(void) { return 0; }
void ser_write_byte(uchar val) {(void)val;}