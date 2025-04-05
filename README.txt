=== CpcGpu ===

GPU Abstractions for Carapace

Still in early stages
See cpc-gpu/cpc-gpu.h for info

Build with:
> meson setup build
> ninja -C build

Take a look at meson_options.txt

To build the example, you must configure like this:
> meson setup build -Depoxy=true -Dexample=true

Running the example:
> ./build/example/cpc-gpu-example

The example requires gtk4 to be installed
