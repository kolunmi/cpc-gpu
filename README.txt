=== CpcGpu ===

GPU Abstractions for Carapace

Still in early stages
See cpc-gpu/cpc-gpu.h for info

Build with:
> meson setup build
> ninja -C build

Take a look at meson_options.txt

To build the example, you must do:
> meson setup build -Depoxy=true -Dexample=true
