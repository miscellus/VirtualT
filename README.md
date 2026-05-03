# Slappy

Slappy is a fork of the awesome VirtualT emulator. Make no mistake, full credit for the emulator goes to the authors of VirtualT!

## Why does this fork exist?
I have a game in the works that targets the NEC PC-8201A. Since I want people to be able to play my game, but not a lot of people have such a machine,
I tought I would host the game on my website in a WebAssembly-based emulator. Instead of writing such a thing from scratch, I decided to use VirtualT as a starting point.

This fork is mostly a simplification of the VirtualT repository, it has fewer features.
For instance, I have removed all of the IDE and authoring parts of the VirtualT project.
I have also removed dependencies on FLTK and a bunch of platform specific #ifdefs and moved to SDL3 for platform abstraction.
I needed to remove all of those things because I plan to use the Emscripten toolchain to compile this emulator for the web platform. To make that work, I need to get rid of all of the platform dependencies.
