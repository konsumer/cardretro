```sh
# start a  watching web-version
npm start

# build & run native
npm run native

# just build target, don't run
npm run web:build
npm run native:build
```

Paths are currently hard-coded, but you cna make it work by setting up `roms/` and `cores/`

```
 cores
├──  fceumm_libretro.dylib
├──  gambatte_libretro.dylib
└──  snes9x_libretro.dylib
 roms
├──  gb
├──  nes
└──  snes
```

You will need [cores](https://buildbot.libretro.com/nightly/) in `cores/`:

- `fceumm`
- `gambatte`
- `snes9x`

For posters, you can run `python fetch_posters.py` and it will save them in `roms/`
