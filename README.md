This is a cardflow loader for libretro cores & roms.

## development

```sh
# start a  watching web-version
npm start

# build & run native
npm run native

# just build target, don't run
npm run web:build
npm run native:build
```

## running

Paths are currently hard-coded, but you cna make it work by setting up `roms/` and `cores/`

```
📁 cores
├── fceumm_libretro.dylib/so/dll
├── gambatte_libretro.dylib/so/dll
└── snes9x_libretro.dylib/so/dll
📁 roms
├── 📁 gb
├── 📁 nes
└── 📁 snes
```

You will need [cores](https://buildbot.libretro.com/nightly/) in `cores/`:

- `fceumm`
- `gambatte`
- `snes9x`

For posters, you can run `python fetch_posters.py` and it will save them in `roms/`

## controls

These are same as [raylib-libretro](https://github.com/RobLoach/raylib-libretro), which this project uses.

| Control            | Keyboard    |
| ---                | ---         |
| D-Pad              | Arrow Keys  |
| Buttons            | ZX AS QW    |
| Start              | Enter       |
| Select             | Right Shift |
| Rewind             | R           |
| Toggle Menu        | F1          |
| Save State         | F2          |
| Load State         | F4          |
| Screenshot         | F8          |
| Previous Shader    | F9          |
| Next Shader        | F10         |
| Fullscreen         | F11         |

When you are in a game, press F1 to go back to cardflow.
