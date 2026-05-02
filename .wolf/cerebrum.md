# Cerebrum

> OpenWolf's learning memory. Updated automatically as the AI learns from interactions.
> Do not edit manually unless correcting an error.
> Last updated: 2026-05-01

## User Preferences

- Web/emscripten build has been dropped entirely. Native-only from here on.

## Key Learnings

- **Project:** cardretro / raylib-cardflow — an emulator frontend using raylib 3D card-flow UI in C.
- **fetch_posters.py strategy:** Compute all CRCs per system → bulk API call to retroapi.konsumer.workers.dev/api/{platform}/crc/{crcs} (100 per request) → rdb_clean_name on returned game name → thumbnail index prefix/fuzzy search → CDN fetch. Falls back to GoodTools filename parsing when CRC not in API.
- **SYSTEM_MAP structure (v5):** maps folder → (api_slug, libretro_dir) tuple. api_slug for retroapi, libretro_dir for thumbnail CDN and index cache.
- **retroapi response format:** array-of-arrays where response[i] = list of matches for crcs[i]. Each match has name, crc, rom_name, etc. — no direct image URL. Game names are same format as RDB names so rdb_clean_name() still applies.
- **RDB naming quirks:** Publisher names like `(Konami)`, Alt markers `(Alt 1)`, hack brackets `[h]`, `[SGB]`, region aliases `(EU-US)`, and "Rev N" appear in base titles — all must be stripped before thumbnail index search.
- **Libretro thumbnail index:** Download from GitHub API `libretro-thumbnails/{repo}/git/trees/HEAD?recursive=1`, cache per-system. Prefix match first, then word-overlap ≥0.85 as fallback.
- **Genuine misses expected:** Japan-only games with fan English names (e.g. "Secret of mana 2" = Seiken Densetsu 3), ROMs with no GoodTools codes and CRC not in RDB.

## Do-Not-Repeat

<!-- Mistakes made and corrected. Each entry prevents the same mistake recurring. -->
<!-- Format: [YYYY-MM-DD] Description of what went wrong and what to do instead. -->

## Decision Log

<!-- Significant technical decisions with rationale. Why X was chosen over Y. -->
