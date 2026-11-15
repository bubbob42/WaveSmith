# WaveSmith

WaveSmith trims a recording's ends by eye and ear, on AmigaOS 3.2. It opens
a recording -- a record side, a tape, a radio show -- finds where the music
starts and ends, shows the lead-in and the run-out shaded with In and Out
set before and after them, plays both ends, and saves the file anew
without the shaded parts. It also cuts parts out, splits a side into its
tracks at markers with a cue sheet beside them, and puts fades and gain on
what it keeps.

Version **0.4** (6.11.2026). English and German, an AmigaGuide manual in
both. It plays through AHI 4 or a Prisma Megamix card; files other than WAV
are decoded on the card.

The GlowIcons and the picture in the About window are by Martin "Mason"
Merz.

## Building

On macOS (the setup of the other projects of this family):

* `m68k-amigaos-gcc` under `~/amiga-arm` (libnix)
* SAS/C 6.58 under `~/Developer/AmigaSDK/SASC/SC`, run through
  [amitools](https://github.com/cnvogelg/amitools)' vamos
* Python 3 for the tools in `tools/` (standard library only)

```sh
gmake            # both compilers, the host tests, the checks, dist/ staged
gmake gcc        # dist/gcc/WaveSmith
gmake sasc       # dist/sasc/WaveSmith
gmake test       # the host unit tests, waveform.gadget drawn on the host
gmake checks     # function signatures, the guides, GCC's epilogues
```

## Releasing

```sh
gmake release           # bumprev: the next revision, then everything again
gmake release-archive   # dist/WaveSmith.lha: Install, the readme, Files/
gmake release-aminet    # dist/aminet: the archive and its readme, checked
```

The package's Installer (`install/Install`) puts WaveSmith into a drawer of
its own (SYS:Tools/WaveSmith by default) with its manuals and catalogs, and
offers Mason's GlowIcons or the classic icons. The Aminet readme is made
from `install/WaveSmith.readme.in` with the version from
`WaveSmith/WaveSmith_rev.h`.

## Testing in FS-UAE

```sh
gmake autotest T=EditTrim W=pal LIMIT=780
gmake autotest T=WaveSmithInstall W=rtg32 LIMIT=300
gmake autotest T=WaveSmithDrawers W=pal LIMIT=180
```

WaveSmithInstall runs the package's Installer (on rtg32 only: its clicks
are measured there), compares what it installed with the package and
opens the installed drawers on Workbench; WaveSmithDrawers lays the same
drawers out on any machine, the tightest being PAL (640x256, Topaz 8).
Every drawer of the package, and of an installation, is arranged by
`tools/arrange_drawers.py`.

Each test in `test/auto/` runs unattended on a copy of the system disk
(`fsuae/local/`, made from `~/Developer/AmigaEmu/HDF/AmigaOS32.hdf`), drives
WaveSmith through `test/rexx/` and the tools in `test/tools/`, and writes
`dist/Test/auto.log`.

## Layout

| Path | What |
|---|---|
| `WaveSmith/` | the program |
| `Waveform/`, `Include/gadgets/waveform.h` | waveform.gadget, the BOOPSI class it is built around |
| `Common/` | modules shared with the Prisma Megamix software it came from |
| `Include/`, `SDK/ahi/` | the Prisma libraries' and AHI's headers |
| `guide/`, `catalogs/` | the manuals (UTF-8 here, Latin-1 when staged) and the translation |
| `icons/glow/`, `art/` | Mason's GlowIcons and About picture, as he delivered them |
| `icons/install/` | AmigaOS 3.2's standard installer icon (GlowIcon and classic), merged into the package's Install icon |
| `icons/readme/` | Mason's About GlowIcon from his AmigaOS 3.2 icon pack, merged onto the readme's icon |
| `icons/drawer/`, `icons/guide/` | AmigaOS 3.2's `def_drawer.info` and `def_amigaguide.info`, verbatim; their GlowIcons go onto Help's drawers and the manuals in the GlowIcon set |
| `install/` | the Installer script and the readme's template |
| `test/` | host tests, FS-UAE autotests and their helpers |

WaveSmith was developed in the Prisma Megamix repository (PrismaNG) as
PrismaEdit and later WaveSmith; its history came along when it moved here.

## Licence

WaveSmith is free software under the GNU Lesser General Public License,
version 2.1 or (at your option) any later version ([COPYING.LIB](COPYING.LIB)).
Mason's art, AmigaOS 3.2's own icons, the Prisma libraries' headers and
AHI's headers are not covered and keep their owners' terms; [LICENSE](LICENSE)
names the files.
