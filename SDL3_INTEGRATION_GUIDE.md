/* display_integration_guide.md - How to integrate SDL3 backend with existing VirtualT */

# Integration Guide for SDL3 Backend

## Current Status

The SDL3 backend is a complete, standalone implementation that:
- ✅ Provides full display rendering
- ✅ Handles keyboard input events
- ✅ Supports headless mode
- ✅ Can run CPU emulation loop
- ✅ Maintains clean separation from emulation core

## Integration Steps

### Step 1: Add Source Files

Add these files to your build system:

```
src/
├── display_sdl3.h
├── display_sdl3.cpp
├── display_wrapper.h
├── display_wrapper.cpp
├── main_sdl3.cpp
└── ... (existing emulation files)
```

### Step 2: Update Build Configuration

**Option A: Use SDL3-only build**

```bash
cmake -f CMakeLists_SDL3.txt .
make
./virtualt_sdl3
```

**Option B: Integrate with existing CMakeLists.txt**

Add to your main `CMakeLists.txt`:

```cmake
# SDL3 support
option(USE_SDL3_BACKEND "Build with SDL3 backend" ON)

if(USE_SDL3_BACKEND)
    find_package(SDL3 REQUIRED)
    set(DISPLAY_SOURCES
        src/display_sdl3.cpp
        src/display_wrapper.cpp
    )
    target_link_libraries(virtualt SDL3::SDL3)
else()
    set(DISPLAY_SOURCES
        src/display.cpp
    )
    target_link_libraries(virtualt fltk)
endif()

target_sources(virtualt PRIVATE ${DISPLAY_SOURCES})
```

### Step 3: Connect Emulation Core

The emulation core needs minimal changes. Ensure these symbols are exported:

```cpp
extern "C" {
    /* Preferences and initialization */
    void init_pref(void);
    void init_mem(void);
    void init_cpu(void);
    
    /* CPU execution */
    void run_cpu(int cycle_count);
    
    /* Display updates from emulation */
    void drawbyte(int driver, int column, int value);
    void lcdcommand(int driver, int value);
    void power_down(void);
    
    /* Global state */
    extern int gModel;
    extern int gRomSize;
}
```

### Step 4: Implement Display Callbacks

The emulation core calls display functions. These should be implemented as:

```cpp
/* In display_sdl3.cpp or a wrapper */

void drawbyte(int driver, int col, uchar value)
{
    /* Render a byte of LCD data */
    for (int y = 0; y < 8; y++) {
        int pixel = (value >> y) & 1;
        display_render_pixel(
            (driver % 5) * 50 + (col & 0x3F),
            ((col >> 6) * 8) + y,
            pixel
        );
    }
}

void lcdcommand(int driver, int value)
{
    /* Handle LCD command (if needed) */
}

void power_down(void)
{
    /* Display goes blank when powered off */
    display_clear_screen();
}
```

### Step 5: Handle Keyboard Input

In `display_sdl3.cpp`, the event handler needs to route keyboard events:

```cpp
void display_handle_events(void)
{
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                /* Map SDL3 key to emulation key code */
                process_key_down(event.key.key);
                break;
                
            case SDL_EVENT_KEY_UP:
                process_key_up(event.key.key);
                break;
                
            case SDL_EVENT_QUIT:
                g_display.running = 0;
                break;
        }
    }
}
```

### Step 6: Choose Entry Point

**Option A: Separate executable**

Keep `main_sdl3.cpp` as a standalone entry point. Original FLTK version remains unchanged.

```bash
# Run original FLTK version
./virtualt

# Run SDL3 version
./virtualt_sdl3
```

**Option B: Unified executable with backend selection**

Modify main() to accept backend argument:

```cpp
int main(int argc, char *argv[])
{
    const char *backend = getenv("VIRTUALT_BACKEND") ?: "sdl3";
    
    if (strcmp(backend, "fltk") == 0) {
        return run_fltk_main(argc, argv);
    } else if (strcmp(backend, "sdl3") == 0) {
        return run_sdl3_main(argc, argv);
    }
    // ...
}
```

Usage:
```bash
VIRTUALT_BACKEND=sdl3 ./virtualt
VIRTUALT_BACKEND=fltk ./virtualt
```

## Key Differences from FLTK Version

### Display Rendering

**FLTK (class-based):**
```cpp
class T100_Disp : public Fl_Widget {
    virtual void draw();
    virtual int handle(int event);
};
```

**SDL3 (function-based):**
```cpp
void display_render_pixel(int x, int y, int is_set);
void display_update_frame(void);
void display_process_input(void);
```

### Event Handling

**FLTK:**
- Callback-based
- Widget hierarchy
- System event loop

**SDL3:**
- Poll-based
- Linear event queue
- Manual event loop

### Color Management

**FLTK:**
- Uses FLTK color constants (FL_BLACK, etc.)

**SDL3:**
- Uses ARGB8888 (0xAARRGGBB)
- More flexible color control

## Testing Checklist

- [ ] SDL3 window opens successfully
- [ ] LCD display renders correctly
- [ ] Keyboard input works
- [ ] Mouse wheel events (if needed)
- [ ] Window resize/fullscreen
- [ ] FPS counter accurate
- [ ] No memory leaks (valgrind)
- [ ] CPU continues running smoothly
- [ ] Headless mode works

## Performance Optimization Tips

1. **Use streaming textures** (already implemented)
   ```cpp
   SDL_TEXTUREACCESS_STREAMING  /* Fast for CPU updates */
   ```

2. **Batch rendering** when possible
   ```cpp
   /* Instead of pixel-by-pixel */
   display_render_rect(x, y, w, h, value);
   ```

3. **Limit frame rate**
   ```cpp
   #define TARGET_FPS 30
   ```

4. **Use efficient pixel format**
   ```cpp
   SDL_PIXELFORMAT_ARGB8888  /* Hardware accelerated */
   ```

## Debugging

### Print Debug Info

```cpp
/* In main_sdl3.cpp */
printf("Display: %dx%d, Scale: %d\n", 
       g_display.pixel_width,
       g_display.pixel_height,
       g_display.scale);
printf("FPS: %.1f\n", current_fps);
```

### Check Event Queue

```cpp
void display_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        printf("Event type: %d\n", event.type);
        // ...
    }
}
```

### Memory Profiling

```bash
valgrind --leak-check=full ./virtualt_sdl3
```

## Common Issues & Solutions

### Issue: SDL3 library not found

**Solution:**
```bash
# Install development files
sudo apt-get install libsdl3-dev

# Or specify path
cmake -DSDL3_DIR=/usr/local/lib/cmake/SDL3 .
```

### Issue: Display not updating

**Solution:** Ensure `display_update_frame()` is called in main loop:
```cpp
while (display_is_running()) {
    run_cpu(40000);
    display_update_frame();  /* Must be called */
}
```

### Issue: Keyboard events not working

**Solution:** Ensure window has focus and event handlers are called:
```cpp
display_process_input();  /* Call every frame */
```

### Issue: Poor performance/lag

**Solution:** 
- Check FPS counter
- Reduce scale factor
- Profile with `perf` or `gprof`
- Check CPU cycles per frame

## Next Steps

1. Build with SDL3
2. Run basic test: `./virtualt_sdl3`
3. Test emulation: Load a ROM and verify display
4. Benchmark performance
5. Integrate with your build system
6. Add Raylib backend (optional)

## Questions?

Refer to:
- `SDL3_BACKEND.md` - Full technical documentation
- `display_wrapper.h` - API reference
- Example in `main_sdl3.cpp` - Reference implementation

---

Happy emulating! 🚀
