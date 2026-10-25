# runtime-patches

This game's own changes to the submodules, applied on top of N64Recomp-browser's patches, in order of name, by
`apply.cmake` here when `CMakeLists.txt` configures and when `tools/build_n64recomp.sh` runs. A new change is a new file per submodule, `<submodule>-<n>-<what>.patch`, numbered after
that submodule's highest, with a line here.

```
rt64-0001    F3DFLX2, the game's own microcode: drawn as F3DEX2 (RT64's database knows it but types it Unknown, so
             its lists ran as RDP commands), with its alpha light (MOVEMEM light 1) and its texture gen, which sets
             each vertex's alpha from the table G_DMA_IO points at by the normal's angle to that light and leaves its
             texture coordinates alone; the machines' paint and shading come from it. Frame matching pairs look ats
             only for vertices that have one.
rt64-0002    A camera's interpolation: the camera's own transform, the view's inverse, is what is blended between two
             game frames, not the view. The view's translation is the eye in the view's axes, so blending it put the
             eye of a camera far from the course's origin (12,000 units) up to 25 units off the line between its two
             places as it turned, and the machine in front of it swung in the frames between.
```
