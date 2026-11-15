PROJECT_NAME := $(notdir $(CURDIR))

# =============================================================================
# WaveSmith -- GCC + SAS/C build
# =============================================================================
#
# WaveSmith trims a recording's ends by eye and ear (AmigaOS 3.2, ReAction),
# around waveform.gadget (Waveform/). Built here on its own; it grew up in
# the Prisma Megamix repository (PrismaNG) and its history came along.
#
#   gmake              both compilers, the host tests, the checks, the staging
#   gmake gcc | sasc   one compiler
#   gmake test         the host unit tests (and waveform.gadget drawn on the host)
#   gmake checks       function signatures, the guides, GCC's epilogues
#   gmake release      bumprev (WaveSmith/WaveSmith_rev.*), then everything again
#   gmake release-stage    build/release/WaveSmith: the package (Install, readme, Files)
#   gmake release-drawer   the same in dist/Release (DEV:Release in FS-UAE)
#   gmake release-archive  dist/WaveSmith.lha (Amiga LhA under vamos)
#   gmake release-aminet   dist/aminet: the archive and its readme, checked -- NOT uploaded
#   gmake autotest T=<test/auto name> [W=pal|pal4|aga32|aga256|rtg32] [LIMIT=secs]
#   gmake run-fsuae    the A1200 + RTG, interactive, dist/ as DEV:
#   gmake clean
#
# Output: dist/sasc/WaveSmith and dist/gcc/WaveSmith, each with its icon and
# its manuals (PROGDIR:Help), so either can be started from DEV: in FS-UAE.
# Objects per profile under build/, header dependencies tracked (GCC -MMD;
# for SAS/C, GCC's preprocessor writes them after sc succeeds), compiler
# flags stamped so a flag change rebuilds exactly its profile. All vamos
# starts go through one lock, so SAS/C stays sequential under -j.
#
# V=1 shows the full command lines.

JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(JOBS) --output-sync=target --no-print-directory

ifneq ($(filter clean,$(MAKECMDGOALS)),)
.NOTPARALLEL:
endif

# a failed recipe must not leave a target that looks up to date
.DELETE_ON_ERROR:

ifeq ($(V),1)
Q :=
else
Q := @
endif

# --- toolchains ----------------------------------------------------------------
PREFIX ?= $(HOME)/amiga-arm

GCC_CC      := $(PREFIX)/bin/m68k-amigaos-gcc
GCC_OBJDUMP := $(PREFIX)/bin/m68k-amigaos-objdump

VAMOS     ?= $(HOME)/Developer/AmigaTools/venv-amitools/bin/vamos
VAMOSRC   := .vamosrc
SASC_PATH ?= $(HOME)/Developer/AmigaSDK/SASC/SC
BUMPREV   ?= $(HOME)/Developer/AmigaTools/bumprev/bumprev

PYTHON ?= python3
HOSTCC ?= cc

# One lock for every vamos start, shared by all projects of this user.
VAMOS_LOCK  := /usr/bin/lockf -k /tmp/amiga-vamos-$(USER).lock
VAMOS_SC    := $(VAMOS_LOCK) tools/vamos-retry $(VAMOS) -c $(VAMOSRC) -q -H disable -m 16128
VAMOS_RAW   := $(VAMOS) -c $(VAMOSRC) -q -H disable -m 16128

# slink can crash or hang in its exit path after writing a complete file.
SLINK_GUARD := tools/slink-guard
SLINK_LIMIT := 90

# slink with undefined symbols asks for DEFINE values on stdin and then writes
# a file anyway, with the references pointing at nothing: fail the link.
# $(1) objects  $(2) libraries
define SLINK_CHECKED
	@mkdir -p $(@D) build/slink
	@echo "slink    $@"
	$(Q)$(VAMOS_LOCK) $(SLINK_GUARD) $@ $(SLINK_LIMIT) $(VAMOS_RAW) slink FROM $(patsubst proj:lib:%,lib:%,$(addprefix proj:,$(1))) TO proj:$@ LIB $(2) $(SLINK_OPTS) \
	    < /dev/null > build/slink/$(subst /,_,$@).log 2>&1; rc=$$?; \
	    grep -v "^$$" build/slink/$(subst /,_,$@).log | grep -v "^Slink - Version\|^Copyright\|SLINK Complete\|Final output" || true; \
	    if grep -q "Undefined symbols" build/slink/$(subst /,_,$@).log; then rm -f $@; echo "slink: undefined symbols in $@"; exit 1; fi; \
	    exit $$rc
endef

# --- flags ---------------------------------------------------------------------
CPUFLAGS := -m68000

GCC_WARNINGS := -Wall -Wextra -Wno-pointer-sign -Wno-int-conversion \
                -Werror=implicit-function-declaration

# An ordinary program with the C startup (libnix / lib:c.o). -msasregparm:
# register arguments, SAS/C's PARAMETERS=REGISTERS -- every function keeps
# within 2 pointers + 2 integers (tools/check_signatures.py), so none goes
# on the stack, where GCC's m68k epilogue miscompile lives (`gmake checks`
# looks for it in the objects). NOSTACKCHECK: SAS/C's stack probe compares
# SP with the MAIN stack's bound, which a process of its own trips over.
# STRMERGE: string literals in the code hunk -- the near data (A4, 64K for
# all data and BSS together) is all but full.
GCC_PROG_CFLAGS  := $(GCC_WARNINGS) -Os -std=gnu99 -noixemul $(CPUFLAGS) -msasregparm -IInclude
GCC_PROG_LDFLAGS := -noixemul $(CPUFLAGS) -msasregparm
SC_PROG_OPTS     := NOVERSION NOICONS NOSTACKCHECK PARAMETERS=REGISTERS STRMERGE OPTIMIZE OPTIMIZERSIZE

SLINK_OPTS := NOICONS SC SD ND QUIET

HOSTCFLAGS ?= -O2 -std=c89 -pedantic -Wall -Wextra

# --- components ------------------------------------------------------------------
# waveform.gadget (Waveform/, Include/gadgets/waveform.h): the class and its
# pure modules, linked into WaveSmith
WAVE_MODS   := waveform waveplan wavepeaks wavesource
WAVE_GCCINC := -IInclude -IWaveform -ICommon
WAVE_SCINC  := IDIR=include: IDIR=proj:Include/ IDIR=proj:Waveform/ IDIR=proj:Common/

# Common/: the modules WaveSmith shares with the Prisma software it came
# from, copied here (2026-11-06): the gap detector and the crackle-proof
# level (gapfind, looklevel, levelmap, level), the cue sheet, the clock,
# the text helpers, Normalise's peak (normwav) and the titled group frame
COMMON_MODS   := gapfind looklevel levelmap level textutil clock cuesheet normwav groupframe
COMMON_GCCINC := -IInclude -ICommon
COMMON_SCINC  := IDIR=include: IDIR=proj:Include/ IDIR=proj:Common/

# WaveSmith itself (WaveSmith/, Documentation/Concept-WaveSmith-2026-10.md)
WS_MODS   := main gui doc trimplan play ahiplay decode mapcut prefetch ask timeline undo editprefs settings margins marks \
             wavesmith_locale help envelope gaincalc gainwin beside headsprite trimwin keyglyph about
WS_GCCINC := -IInclude -IWaveSmith -IWaveform -ICommon -ISDK/ahi/include
WS_SCINC  := IDIR=include: IDIR=proj:Include/ IDIR=proj:WaveSmith/ IDIR=proj:Waveform/ IDIR=proj:Common/ IDIR=proj:SDK/ahi/include/

# --- main targets ----------------------------------------------------------------
.PHONY: all gcc sasc test checks stage clean FORCE

all: gcc sasc test checks stage

gcc: dist/gcc/WaveSmith
sasc: dist/sasc/WaveSmith

# --- flag stamps -------------------------------------------------------------------
FORCE:

define FLAG_STAMP
$(1): FORCE
	@mkdir -p $$(@D)
	@if [ "`cat $$@ 2>/dev/null`" != "$(2)" ]; then printf '%s' "$(2)" > $$@; fi
endef

$(eval $(call FLAG_STAMP,build/gcc/.flags,$(GCC_PROG_CFLAGS)))
$(eval $(call FLAG_STAMP,build/sasc/.flags,$(SC_PROG_OPTS)))

# --- object rules ------------------------------------------------------------------
# $(1) profile  $(2) component  $(3) source dir  $(4) compiler flags
define GCC_OBJ
build/$(1)/$(2)/%.o: $(3)/%.c build/$(1)/.flags
	@mkdir -p $$(@D)
	@echo "gcc $(1)	$$<"
	$$(Q)$$(GCC_CC) $(4) -MMD -MP -c $$< -o $$@
endef

# $(1) profile  $(2) component  $(3) source dir  $(4) sc options  $(5) GCC -I for deps
# (sc has no -M: GCC's preprocessor writes the dependencies after sc succeeds)
# SC_FILE_OPTS: an object's own options (target-specific), e.g. CODE=FAR
# for a module past 32K of code -- its calls must reach slink's link
# vectors at its end (Error 502: Reloc16 > 32768)
define SC_OBJ
build/$(1)/$(2)/%.o: $(3)/%.c build/$(1)/.flags | $$(VAMOSRC)
	@mkdir -p $$(@D)
	@echo "sc  $(1)	$$<"
	$$(Q)$$(VAMOS_SC) sc $(4) $$(SC_FILE_OPTS) NOLINK OBJNAME=proj:$$@ proj:$$<
	$$(Q)$$(GCC_CC) -noixemul $(5) -MM -MP -MT $$@ -MF $$(@:.o=.d) $$<
endef

$(eval $(call GCC_OBJ,gcc,wave,Waveform,$(GCC_PROG_CFLAGS) $(WAVE_GCCINC)))
$(eval $(call SC_OBJ,sasc,wave,Waveform,$(SC_PROG_OPTS) $(WAVE_SCINC),$(WAVE_GCCINC)))
$(eval $(call GCC_OBJ,gcc,common,Common,$(GCC_PROG_CFLAGS) $(COMMON_GCCINC)))
$(eval $(call SC_OBJ,sasc,common,Common,$(SC_PROG_OPTS) $(COMMON_SCINC),$(COMMON_GCCINC)))
$(eval $(call GCC_OBJ,gcc,edit,WaveSmith,$(GCC_PROG_CFLAGS) $(WS_GCCINC)))
$(eval $(call SC_OBJ,sasc,edit,WaveSmith,$(SC_PROG_OPTS) $(WS_SCINC),$(WS_GCCINC)))
# gui.c is past 32K of code
build/sasc/edit/gui.o: SC_FILE_OPTS := CODE=FAR

-include $(wildcard build/*/*/*.d)

$(VAMOSRC): .vamosrc.in
	sed -e 's|@SASC_PATH@|$(SASC_PATH)|g' -e 's|@PROJECT_PATH@|$(CURDIR)|g' .vamosrc.in > $(VAMOSRC)

# --- link rules --------------------------------------------------------------------
# object lists: $(call objs,profile,component,modules)
objs = $(addprefix build/$(1)/$(2)/,$(addsuffix .o,$(3)))
WS_LINKED = $(call objs,$(1),edit,$(WS_MODS)) $(call objs,$(1),wave,$(WAVE_MODS)) $(call objs,$(1),common,$(COMMON_MODS))

dist/gcc/WaveSmith: $(call WS_LINKED,gcc)
	@mkdir -p $(@D)
	@echo "link     $@"
	$(Q)$(GCC_CC) $(GCC_PROG_LDFLAGS) -s $^ -o $@ -lamiga -Wl,-Map,build/gcc/edit/WaveSmith.map

dist/sasc/WaveSmith: $(call WS_LINKED,sasc) | $(VAMOSRC)
	$(call SLINK_CHECKED,lib:c.o $^,lib:sc.lib lib:amiga.lib)

# --- the manuals, the catalogs, the icons --------------------------------------------
# One manual per language, guide/<language>/WaveSmith.guide (UTF-8, to edit
# on the host) as dist/Help/<language>/ (Latin-1, as the Amiga reads it),
# and beside each build (PROGDIR:Help); one catalog per translation, built
# on the host from the .cd and the .ct (tools/make_catalog.py: CatComp's own
# format) after the .ct is checked against the seed (tools/gen_catalog.py).
# NOT beside the builds: the test disk speaks German, and the autotests
# click where the ENGLISH windows have their gadgets -- EditGerman copies
# the catalog into RAM: for its own run.
#
# A change of WaveSmith/wavesmith_strings.h needs
#   python3 tools/gen_catalog.py --name WaveSmith --prefix WS --srcdir WaveSmith gen
# which rewrites the .cd and the headers and restamps every .ct.
WS_GUIDES        := $(patsubst guide/%/WaveSmith.guide,dist/Help/%/WaveSmith.guide,$(wildcard guide/*/WaveSmith.guide))
WS_CATALOGS      := $(patsubst catalogs/%/WaveSmith.ct,dist/Catalogs/%/WaveSmith.catalog,$(wildcard catalogs/*/WaveSmith.ct))
WS_PROGDIR_FILES := $(foreach d,sasc gcc,$(addprefix dist/$(d)/,$(patsubst dist/%,%,$(WS_GUIDES))))
WS_CATOPTS       := --name WaveSmith --prefix WS --srcdir WaveSmith

dist/Help/%/WaveSmith.guide: guide/%/WaveSmith.guide tools/check_guide.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/check_guide.py $< >/dev/null
	$(Q)iconv -f UTF-8 -t ISO-8859-1 $< > $@
	@echo "stage    $@"

dist/Catalogs/%/WaveSmith.catalog: catalogs/%/WaveSmith.ct catalogs/WaveSmith.cd \
                                   tools/gen_catalog.py tools/make_catalog.py WaveSmith/wavesmith_strings.h
	@mkdir -p $(@D)
	$(Q)out=`$(PYTHON) tools/gen_catalog.py $(WS_CATOPTS) check $<` || { echo "$$out"; exit 1; }
	$(Q)$(PYTHON) tools/make_catalog.py catalogs/WaveSmith.cd $< $@ >/dev/null
	@echo "catalog  $@"

dist/sasc/Help/%: dist/Help/%
	@mkdir -p $(@D)
	$(Q)cp $< $@
dist/gcc/Help/%: dist/Help/%
	@mkdir -p $(@D)
	$(Q)cp $< $@

# WaveSmith's icon (tools/make_edit_icons.py: cropped to its artwork), with
# the GlowIcon by Martin "Mason" Merz on it: OUR classic icon with HIS IFF
# FORM ICON appended at its exact end (tools/merge_glowicon.py), so
# icon.library V44+ draws his art and anything older our classic icon, and
# the stack and tooltypes are ours either way. icons/glow/ keeps his
# originals; merged here, never committed merged. The package's other icons
# come from the same tool: the drawer, the readme, the Installer, a manual.
build/edit-icons/.stamp: tools/make_edit_icons.py tools/package_icons.py tools/amiga_icon.py tools/icon_crop.py \
                         icons/install/Install-classic.info
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/make_edit_icons.py $(@D)
	@touch $@

# $(1) the merged icon's name  $(2) our classic one in build/edit-icons
# $(3) the GlowIcon it takes the colour images from
define GLOW_MERGE
build/edit-icons/glow/$(1): build/edit-icons/.stamp $(3) tools/merge_glowicon.py tools/icon_crop.py
	@mkdir -p $$(@D)
	$$(Q)$$(PYTHON) tools/merge_glowicon.py build/edit-icons/$(2) $(3) $$@ >/dev/null
	@n=$$$$(wc -c < build/edit-icons/$(2) | tr -d ' '); \
	head -c $$$$n $$@ | cmp -s - build/edit-icons/$(2) || { echo "GLOW $$@ does not start with our icon"; rm -f $$@; exit 1; }; \
	dd if=$$@ bs=1 skip=$$$$n count=4 2>/dev/null | grep -q FORM || { echo "GLOW $$@ has no IFF at offset $$$$n"; rm -f $$@; exit 1; }
	@echo "glow     $$@ (a superset of our icon)"
endef
$(eval $(call GLOW_MERGE,WaveSmith.info,WaveSmith.info,icons/glow/WaveSmith.info))
$(eval $(call GLOW_MERGE,WaveSmithDrawer.info,Drawer.info,icons/glow/WaveSmithDrawer.info))
# the package's Install icon: AmigaOS 3.2's standard installer icon, its
# classic part (icons/install/Install-classic.info, made ours by
# make_edit_icons.py) and its GlowIcon
$(eval $(call GLOW_MERGE,Install.info,Install.info,icons/install/Install-glow.info))
# the readme's: our classic text icon (MultiView) and Mason's About GlowIcon
# from his AmigaOS 3.2 icon pack (icons/readme/Readme-glow.info)
$(eval $(call GLOW_MERGE,Readme.info,Readme.info,icons/readme/Readme-glow.info))
# the GlowIcon set's Help drawers and manuals: our classic drawer and page
# with AmigaOS 3.2's own GlowIcons (ENVARC:Sys/def_drawer.info and
# def_amigaguide.info, kept verbatim in icons/drawer and icons/guide)
$(eval $(call GLOW_MERGE,Folder.info,Folder.info,icons/drawer/def_drawer.info))
$(eval $(call GLOW_MERGE,Guide.info,Guide.info,icons/guide/def_amigaguide.info))
WS_GLOW         := build/edit-icons/glow/WaveSmith.info
WS_DRAWER_GLOW  := build/edit-icons/glow/WaveSmithDrawer.info
WS_INSTALL_GLOW := build/edit-icons/glow/Install.info
WS_README_GLOW  := build/edit-icons/glow/Readme.info
WS_FOLDER_GLOW  := build/edit-icons/glow/Folder.info
WS_GUIDE_GLOW   := build/edit-icons/glow/Guide.info

dist/sasc/WaveSmith.info dist/gcc/WaveSmith.info: $(WS_GLOW)
	@mkdir -p $(@D)
	$(Q)cp $(WS_GLOW) $@

# --- the staging: dist/ as DEV: in FS-UAE --------------------------------------------
#   EditTry   WaveSmith by hand on the test single in RAM:WaveSmith; what to
#             try in EditTry.txt (paged); an IconX script with its icon
#   mutools   MuForce and MuGuardianAngel from SYS:Tools/mutools (the tests)
stage: dist/sasc/WaveSmith.info dist/gcc/WaveSmith.info $(WS_GUIDES) $(WS_CATALOGS) $(WS_PROGDIR_FILES) \
       dist/EditTry dist/EditTry.txt dist/mutools

dist/EditTry: test/amiga/EditTry tools/make_test_icons.py tools/amiga_icon.py
	@mkdir -p dist
	@cp $< $@
	$(Q)$(PYTHON) tools/make_test_icons.py dist EditTry >/dev/null
	@echo "stage    $@ (+ icon)"

dist/EditTry.txt: test/amiga/EditTry.txt
	@mkdir -p $(@D)
	@cp $< $@

dist/mutools: test/amiga/mutools
	@mkdir -p $(@D)
	cp $< $@

# --- host tests --------------------------------------------------------------------
# Common/'s modules: test/host/common/test_<name>.c against the modules it
# names, with check.h and a shim of the system headers
COMMON_TESTS   := test_level test_levelmap test_gapfind test_looklevel
COMMON_HOSTINC := -Itest/host/common/shim -Itest/host/common -ICommon -IInclude

define COMMON_TEST
build/host/common/$(1): test/host/common/$(1).c $(addprefix Common/,$(addsuffix .c,$(2))) \
                        $(wildcard Common/*.h) test/host/common/check.h
	@mkdir -p $$(@D)
	@echo "cc       $$@"
	$$(Q)$$(HOSTCC) $$(HOSTCFLAGS) $$(COMMON_HOSTINC) -o $$@ $$(filter %.c,$$^) -lm
endef
$(eval $(call COMMON_TEST,test_level,level))
$(eval $(call COMMON_TEST,test_levelmap,levelmap level))
$(eval $(call COMMON_TEST,test_gapfind,gapfind))
$(eval $(call COMMON_TEST,test_looklevel,looklevel levelmap level))

# Normalise's peak (Common/normwav.c), pure
build/host/common/test_normwav: test/host/common/test_normwav.c Common/normwav.c Common/normwav.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -DPRISMA_HOST -ICommon -o $@ test/host/common/test_normwav.c Common/normwav.c -lm

# waveform.gadget's pure modules (Waveform/): test/host/waveform/test_<name>.c
# against the modules it names; the common shim and check.h, and a shim of
# the three system headers gadgets/waveform.h includes
WF_TESTS   := test_wavepeaks test_waveplan
WF_HOSTINC := -Itest/host/waveform/shim -Itest/host/common/shim -Itest/host/common -IWaveform -IInclude

define WF_TEST
build/host/waveform/$(1): test/host/waveform/$(1).c $(addprefix Waveform/,$(addsuffix .c,$(2))) \
                          $(wildcard Waveform/*.h) Include/gadgets/waveform.h test/host/common/check.h
	@mkdir -p $$(@D)
	@echo "cc       $$@"
	$$(Q)$$(HOSTCC) $$(HOSTCFLAGS) $$(WF_HOSTINC) -o $$@ $$(filter %.c,$$^)
endef
$(eval $(call WF_TEST,test_wavepeaks,wavepeaks))
$(eval $(call WF_TEST,test_waveplan,waveplan))

# WaveSmith's pure modules: test/host/wavesmith/test_<name>.c
WS_TESTS   := test_trimplan test_docsave test_timeline test_undo test_editprefs test_cuesheet test_marks test_envelope test_gaincalc
WS_HOSTINC := -Itest/host/waveform/shim -Itest/host/common/shim -Itest/host/common -IWaveSmith -ICommon -IWaveform -IInclude
build/host/wavesmith/test_trimplan: test/host/wavesmith/test_trimplan.c WaveSmith/trimplan.c Common/gapfind.c \
                                     WaveSmith/trimplan.h Common/gapfind.h Include/gadgets/waveform.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)

# doc.c's saving against dos.library on the host (doshost.c), which fails
# on demand; its files in build/host/wavesmith/docsave. wavesource.c sets
# its hooks as the Amiga's headers declare them, (ULONG (*)()): the host's
# clang calls that unprototyped, for this one test told not to
WS_DOCSAVE := test/host/wavesmith/test_docsave.c test/host/wavesmith/doshost.c WaveSmith/doc.c WaveSmith/trimplan.c WaveSmith/mapcut.c WaveSmith/beside.c \
              WaveSmith/timeline.c WaveSmith/envelope.c \
              $(addprefix Waveform/,wavepeaks.c waveplan.c wavesource.c) \
              $(addprefix Common/,gapfind.c looklevel.c levelmap.c level.c textutil.c normwav.c)
# a long file read ends first (Doc_SetEnds): the test single and a worn
# record side read whole and ends first, the same suggestion, picture and
# plain peaks
build/host/wavesmith/side.wav: tools/make_vinyl_side.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/make_vinyl_side.py $(@D)
WS_DOCENDS := $(filter-out test/host/wavesmith/test_docsave.c,$(WS_DOCSAVE)) test/host/wavesmith/test_docends.c
build/host/wavesmith/test_docends: $(WS_DOCENDS) $(wildcard test/host/wavesmith/*.h test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h) \
                                    $(wildcard WaveSmith/*.h Waveform/*.h) Include/gadgets/waveform.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Wno-strict-prototypes -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) \
	    -o $@ $(WS_DOCENDS)
WS_PREFETCH := $(filter-out test/host/wavesmith/test_docsave.c,$(WS_DOCSAVE)) test/host/wavesmith/test_prefetch.c WaveSmith/prefetch.c
build/host/wavesmith/test_prefetch: $(WS_PREFETCH) $(wildcard test/host/wavesmith/*.h test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h) \
                                     $(wildcard WaveSmith/*.h Waveform/*.h) Include/gadgets/waveform.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Wno-strict-prototypes -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) \
	    -o $@ $(WS_PREFETCH)
WS_TIMELINE := test/host/wavesmith/test_timeline.c test/host/wavesmith/doshost.c WaveSmith/timeline.c WaveSmith/envelope.c
build/host/wavesmith/test_timeline: $(WS_TIMELINE) WaveSmith/timeline.h WaveSmith/envelope.h test/host/wavesmith/doshost.h \
                                     $(wildcard test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h) test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) -o $@ $(WS_TIMELINE)
build/host/wavesmith/test_envelope: test/host/wavesmith/test_envelope.c WaveSmith/envelope.c WaveSmith/envelope.h \
                                     test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)
# the levels as read mirrored by a host program, compared by tools/check_level_mirror.py
WS_MIRROR := test/host/wavesmith/level_mirror.c test/host/wavesmith/doshost.c WaveSmith/timeline.c WaveSmith/envelope.c
build/host/wavesmith/level_mirror: $(WS_MIRROR) WaveSmith/timeline.h WaveSmith/envelope.h test/host/wavesmith/doshost.h \
                                    $(wildcard test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h)
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) -o $@ $(WS_MIRROR)
build/host/wavesmith/test_gaincalc: test/host/wavesmith/test_gaincalc.c WaveSmith/gaincalc.c WaveSmith/envelope.c \
                                     Common/normwav.c Common/textutil.c WaveSmith/gaincalc.h WaveSmith/envelope.h \
                                     test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)
build/host/wavesmith/test_marks: test/host/wavesmith/test_marks.c WaveSmith/marks.c WaveSmith/marks.h Common/cuesheet.h \
                                  test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)
# the cue sheet's text (Common/cuesheet.c), textutil under it
build/host/wavesmith/test_cuesheet: test/host/wavesmith/test_cuesheet.c Common/cuesheet.c Common/textutil.c \
                                     Common/cuesheet.h Common/textutil.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)
build/host/wavesmith/test_editprefs: test/host/wavesmith/test_editprefs.c WaveSmith/editprefs.c Common/textutil.c \
                                      WaveSmith/editprefs.h Common/textutil.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(WS_HOSTINC) -o $@ $(filter %.c,$^)
WS_UNDO := test/host/wavesmith/test_undo.c test/host/wavesmith/doshost.c WaveSmith/undo.c WaveSmith/timeline.c WaveSmith/envelope.c
build/host/wavesmith/test_undo: $(WS_UNDO) WaveSmith/undo.h WaveSmith/timeline.h test/host/wavesmith/doshost.h \
                                 $(wildcard test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h) test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) -o $@ $(WS_UNDO)
build/host/wavesmith/test_docsave: $(WS_DOCSAVE) $(wildcard test/host/wavesmith/*.h test/host/wavesmith/shim/*.h test/host/wavesmith/shim/*/*.h) \
                                    $(wildcard WaveSmith/*.h Waveform/*.h) Include/gadgets/waveform.h test/host/common/check.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -Wno-strict-prototypes -Itest/host/wavesmith/shim -Itest/host/wavesmith $(WS_HOSTINC) \
	    -o $@ $(WS_DOCSAVE)

# waveform.gadget drawn on the host (test/host/wavegfx): the class itself
# against stub system headers, WaveSmith's window states painted into a
# frame buffer -- every paint inside its box, the pointer shapes, a drag,
# everything given back -- with pens and without; the frames as PPMs in
# build/host/wavegfx/{pens,nopens}; again on planar bitmaps in chip RAM
# (WAVEGFX_STANDARD: the wave through the class's mask), {pens,nopens}-std,
# every frame the same as the RectFills'. C99: the stubs, not the class
WG_SRC     := test/host/wavegfx/driver.c test/host/wavegfx/gfx.c \
              $(addprefix Waveform/,waveform.c waveplan.c wavepeaks.c wavesource.c) \
              $(addprefix Common/,looklevel.c levelmap.c level.c gapfind.c normwav.c) WaveSmith/trimplan.c
WG_HOSTINC := -Itest/host/wavegfx/stub -IInclude -IWaveform -ICommon -IWaveSmith
WG_CFLAGS  := -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast
build/host/wavegfx/driver: $(WG_SRC) $(wildcard test/host/wavegfx/*.h test/host/wavegfx/stub/*.h test/host/wavegfx/stub/*/*.h) \
                           $(wildcard Waveform/*.h) Include/gadgets/waveform.h
	@mkdir -p $(@D)
	@echo "cc       $@"
	$(Q)$(HOSTCC) $(WG_CFLAGS) $(WG_HOSTINC) -o $@ $(WG_SRC)
build/host/wavegfx/Single.wav: tools/make_edit_single.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/make_edit_single.py $(@D)

test: $(addprefix build/host/common/,$(COMMON_TESTS)) build/host/common/test_normwav $(addprefix build/host/waveform/,$(WF_TESTS)) \
      $(addprefix build/host/wavesmith/,$(WS_TESTS)) build/host/wavesmith/test_docends build/host/wavesmith/side.wav \
      build/host/wavesmith/test_prefetch \
      build/host/wavesmith/level_mirror tools/check_level_mirror.py tools/make_edit_single.py \
      build/host/wavegfx/driver build/host/wavegfx/Single.wav
	@mkdir -p build/host/wavesmith/docsave
	$(Q)for t in $(COMMON_TESTS); do build/host/common/$$t || exit 1; done
	$(Q)build/host/common/test_normwav
	$(Q)for t in $(WF_TESTS); do build/host/waveform/$$t || exit 1; done
	$(Q)for t in $(WS_TESTS); do build/host/wavesmith/$$t build/host/wavesmith/docsave || exit 1; done
	$(Q)build/host/wavesmith/level_mirror build/host/wavesmith/docsave
	$(Q)$(PYTHON) tools/check_level_mirror.py build/host/wavesmith/docsave
	$(Q)build/host/wavesmith/test_docends build/host/wavesmith/docsave build/host/wavegfx/Single.wav build/host/wavesmith/side.wav
	$(Q)build/host/wavesmith/test_prefetch build/host/wavesmith/docsave build/host/wavegfx/Single.wav
	$(Q)mkdir -p build/host/wavegfx/pens build/host/wavegfx/nopens
	$(Q)build/host/wavegfx/driver build/host/wavegfx/Single.wav build/host/wavegfx/pens
	$(Q)build/host/wavegfx/driver build/host/wavegfx/Single.wav build/host/wavegfx/nopens nopens
	$(Q)mkdir -p build/host/wavegfx/pens-std build/host/wavegfx/nopens-std
	$(Q)WAVEGFX_STANDARD=1 build/host/wavegfx/driver build/host/wavegfx/Single.wav build/host/wavegfx/pens-std
	$(Q)WAVEGFX_STANDARD=1 build/host/wavegfx/driver build/host/wavegfx/Single.wav build/host/wavegfx/nopens-std nopens
	$(Q)for m in pens nopens; do for f in build/host/wavegfx/$$m/*.ppm; do \
	    cmp -s $$f build/host/wavegfx/$$m-std/$${f##*/} || { echo "FAIL wavegfx: $$f drawn otherwise through the mask"; exit 1; }; \
	done; done; echo "wavegfx: the mask's frames the RectFills'"

# --- checks ------------------------------------------------------------------------
.PHONY: signatures guidecheck epilogues
checks: signatures guidecheck epilogues

# every function within 2 pointers + 2 integers: beyond them arguments go
# on the stack, where the GCC epilogue miscompile lives
signatures: build/checks/signatures.ok
build/checks/signatures.ok: $(wildcard Waveform/*.c Waveform/*.h WaveSmith/*.c WaveSmith/*.h) Include/gadgets/waveform.h \
                            tools/check_signatures.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/check_signatures.py $(wildcard Waveform/*.h Waveform/*.c WaveSmith/*.h WaveSmith/*.c) Include/gadgets/waveform.h
	@touch $@

# every guide's links land and its attributes close; every page the Help
# key asks for (HELP_NODE("...")) is in every guide
guidecheck: build/checks/guides.ok
build/checks/guides.ok: $(wildcard guide/*/*.guide) $(wildcard WaveSmith/*.c) tools/check_guide.py tools/check_helpnodes.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/check_guide.py guide/*/*.guide
	$(Q)$(PYTHON) tools/check_helpnodes.py --guides guide/*/WaveSmith.guide -- WaveSmith/*.c
	@touch $@

# the GCC m68k epilogue miscompile (docs in tools/check_epilogues.py); the
# self-test proves the checker still catches it before trusting a pass
epilogues: build/checks/epilogues.ok
build/checks/epilogues.ok: $(call WS_LINKED,gcc) tools/check_epilogues.py tools/epilogue-selftest.s
	@mkdir -p $(@D)
	$(Q)$(GCC_CC) -c tools/epilogue-selftest.s -o build/checks/epilogue-selftest.o
	$(Q)if GCC_OBJDUMP=$(GCC_OBJDUMP) $(PYTHON) tools/check_epilogues.py \
	        build/checks/epilogue-selftest.o >/dev/null 2>&1; then \
	    echo "check_epilogues: SELF-TEST FAILED -- it no longer catches the bad epilogue"; \
	    exit 1; \
	fi
	$(Q)GCC_OBJDUMP=$(GCC_OBJDUMP) $(PYTHON) tools/check_epilogues.py $(filter build/gcc%.o,$?)
	@touch $@

# --- release -----------------------------------------------------------------------
# The revision moves ONLY here, on the word of the person releasing: bumprev
# (from WaveSmith/, so the generated includes land beside the sources), then
# everything again so the new $VER is in both builds. A new version is set
# here, in REV_VERSION: bumprev never restarts the revision by itself (it
# would make 0.4 into 1.5), so the first release of a new version writes -1
# into the .rev file and bumprev counts it up to N.0.
REV_VERSION := 1
REV_H       := WaveSmith/WaveSmith_rev.h
REV_FILE    := WaveSmith/WaveSmith_rev.rev

.PHONY: release
release:
	@if ! grep -Eq '^#define[[:space:]]+VERSION[[:space:]]+$(REV_VERSION)$$' $(REV_H); then \
	    echo "release: version $(REV_VERSION) is new -- its revision starts at 0"; \
	    echo -1 >$(REV_FILE); \
	fi
	cd WaveSmith && $(VAMOS_LOCK) $(VAMOS) -q $(BUMPREV) $(REV_VERSION) WaveSmith
	+$(MAKE) all
	@grep VERS WaveSmith/WaveSmith_rev.h | head -1

include release.mk
include fsuae.mk

clean:
	rm -rf build dist .vamosrc
