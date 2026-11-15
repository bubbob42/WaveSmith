# --- FS-UAE machines and the autotests --------------------------------------------------
# (included by GNUmakefile)
#
#   gmake run-fsuae                  the A1200 + RTG, interactive, dist/ as DEV:
#   gmake autotest T=<name> [W=pal|pal4|aga32|aga256|rtg32] [LIMIT=secs]
#                                    test/auto/<name>, unattended, on a COPY of
#                                    the system disk; serial on 1235, so it
#                                    never meets an interactive emulator (1234)
#
# The machine has one emulator and several projects: check `pgrep -x fs-uae`
# before a run, use LIMIT= rather than an external timeout (that orphans the
# emulator), and batch the runs.
FSUAE ?= /opt/homebrew/bin/fs-uae
FSUAE_TEMPLATE := fsuae/machine.fs-uae.in
PROJECT_DIST   := $(CURDIR)/dist
KICKSTART_FILE ?= $(HOME)/Developer/AmigaEmu/ROMs/kick32.rom
SYSTEM_HDF     ?= $(HOME)/Developer/AmigaEmu/HDF/AmigaOS32.hdf
XDFTOOL_BIN    ?= $(HOME)/Developer/AmigaTools/venv-amitools/bin/xdftool
P96_INC        ?= $(HOME)/Developer/AmigaSDK/Picasso96Develop/Include

# $(1) machine suffix  $(2) model  $(3) cpu  $(4) MMU line  $(5) kickstart
# $(6) system drive  $(7) chip KB  $(8) fast KB  $(9) accelerator + memory
# $(10) graphics card + memory  $(11) title
define MACHINE
FSUAE_CONFIG$(1) := fsuae/local/$(PROJECT_NAME)$(1).fs-uae

$$(FSUAE_CONFIG$(1)): $$(FSUAE_TEMPLATE) fsuae.mk
	@mkdir -p fsuae/local
	$$(Q)sed -e 's|@AMIGA_MODEL@|$(2)|' -e 's|@CPU@|$(3)|' -e 's|@MMU_LINE@|$(4)|' \
	    -e 's|@KICKSTART_FILE@|$(5)|' -e 's|@SYSTEM_DRIVE@|$(6)|' \
	    -e 's|@PROJECT_DIST@|$$(PROJECT_DIST)|' \
	    -e 's|@CHIP_MEMORY@|$(7)|' -e 's|@FAST_MEMORY@|$(8)|' \
	    -e 's|@ACCELERATOR@|$(word 1,$(9))|' -e 's|@ACCELERATOR_MEMORY@|$(word 2,$(9))|' \
	    -e 's|@GRAPHICS_CARD@|$(word 1,$(10))|' -e 's|@GRAPHICS_CARD_MEMORY@|$(word 2,$(10))|' \
	    -e 's|@TITLE@|$(11)|' -e 's|@SERIAL_PORT@|$$(if $$(findstring -autotest,$(1)),1235,1234)|' \
	    -e 's|@VOLUME@|$$(if $$(findstring -autotest,$(1)),0,100)|' \
	    $$(FSUAE_TEMPLATE) > $$@

.PHONY: fsuae-config$(1) launch-fsuae$(1) run-fsuae$(1)
fsuae-config$(1): $$(FSUAE_CONFIG$(1))

launch-fsuae$(1): $$(FSUAE_CONFIG$(1))
	@test -f "$(5)" || (echo "Missing Kickstart ROM: $(5)" && exit 1)
	@test -e "$(6)" || (echo "Missing system drive: $(6)" && exit 1)
	@echo "Starting FS-UAE: $(11)"
	@"$$(FSUAE)" "$$(abspath $$(FSUAE_CONFIG$(1)))" >/dev/null 2>&1 &

run-fsuae$(1): all
	+$$(MAKE) launch-fsuae$(1)
endef

MMU030 := uae_mmu_model = 68030

AUTOTEST_HDF        := $(CURDIR)/fsuae/local/autotest-System.hdf
AUTOTEST_HDF_pal    := $(CURDIR)/fsuae/local/autotest-System-pal.hdf
AUTOTEST_HDF_pal4   := $(CURDIR)/fsuae/local/autotest-System-pal4.hdf
AUTOTEST_HDF_rtg32  := $(CURDIR)/fsuae/local/autotest-System-rtg32.hdf
AUTOTEST_HDF_aga32  := $(CURDIR)/fsuae/local/autotest-System-aga32.hdf
AUTOTEST_HDF_aga256 := $(CURDIR)/fsuae/local/autotest-System-aga256.hdf

$(eval $(call MACHINE,,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(SYSTEM_HDF),2048,8192,0 0,uaegfx-z3 16,AmigaOS 3.2 A1200 AGA+RTG - WaveSmith))
$(eval $(call MACHINE,-autotest,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF),2048,8192,0 0,uaegfx-z3 16,WaveSmith autotest))
$(eval $(call MACHINE,-autotest-rtg32,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF_rtg32),2048,8192,0 0,uaegfx-z3 16,WaveSmith autotest RTG32))
# W=pal: PAL hires 640x256, 16 colours, Topaz 8 for every font
$(eval $(call MACHINE,-autotest-pal,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF_pal),2048,8192,0 0,none 0,WaveSmith autotest PAL Hires Topaz 8))
# W=pal4: the same in four colours
$(eval $(call MACHINE,-autotest-pal4,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF_pal4),2048,8192,0 0,none 0,WaveSmith autotest PAL Hires 4 colours))
# W=aga32, W=aga256: PAL hires in 32 and 256 colours, Topaz 8
$(eval $(call MACHINE,-autotest-aga32,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF_aga32),2048,8192,0 0,none 0,WaveSmith autotest AGA 32 colours))
$(eval $(call MACHINE,-autotest-aga256,A1200/020,68030,$(MMU030),$(KICKSTART_FILE),$(AUTOTEST_HDF_aga256),2048,8192,0 0,none 0,WaveSmith autotest AGA 256 colours))

# A COPY of the system disk whose User-Startup runs DEV:Test/AutoRun. The
# daily machine's system disk may carry the Prisma Megamix software: the
# copies lose its User-Startup block and its files, so every test starts
# from a machine that never had them. WaveSmith's own installation in
# SYS:Tools goes too: the Installer's test starts from none.
# (xdftool knows an image by its extension: the temporary copy ends in .hdf)
$(AUTOTEST_HDF):
	@mkdir -p fsuae/local build
	cp "$(SYSTEM_HDF)" $(@:.hdf=-tmp.hdf)
	$(XDFTOOL_BIN) $(@:.hdf=-tmp.hdf) read S/User-Startup build/User-Startup
	sed -e '/^;BEGIN Prisma$$/,/^;END Prisma$$/d' build/User-Startup > build/User-Startup.clean
	mv build/User-Startup.clean build/User-Startup
	printf '\n;BEGIN AutotestHook\nIf EXISTS DEV:Test/AutoRun\n  Run >DEV:Test/AutoRun.out Execute DEV:Test/AutoRun\nEndIf\n;END AutotestHook\n' >> build/User-Startup
	$(XDFTOOL_BIN) $(@:.hdf=-tmp.hdf) delete S/User-Startup + write build/User-Startup S/User-Startup
	-$(XDFTOOL_BIN) $(@:.hdf=-tmp.hdf) delete WBStartup/PrismaControl
	-$(XDFTOOL_BIN) $(@:.hdf=-tmp.hdf) delete WBStartup/PrismaControl.info
	@for f in Tools/Prisma Tools/Prisma.info Tools/WaveSmith Tools/WaveSmith.info Libs/prisma.library Libs/MHI/mhiprisma.library \
	          Devs/AHI/prisma.audio Devs/AudioModes/PRISMA C/PrismaPlay; do \
		$(XDFTOOL_BIN) $(@:.hdf=-tmp.hdf) delete $$f all >/dev/null 2>&1 && echo "  removed $$f (the daily machine's)"; \
	done; true
	mv $(@:.hdf=-tmp.hdf) $@

# One prefs file per variant: two rules writing one build/ScreenMode.prefs
# under -j raced. Each disk's mode is read back from the disk before it is kept.
# $(1) variant  $(2) mode id  $(3) width  $(4) height  $(5) depth  $(6) topaz 8 (1) or not
define VARIANT_HDF
$$(AUTOTEST_HDF_$(1)): $$(AUTOTEST_HDF) tools/make_font_prefs.py tools/make_screenmode_prefs.py
	cp $$< $$(@:.hdf=-tmp.hdf)
	$$(PYTHON) tools/make_screenmode_prefs.py build/ScreenMode-$(1).prefs $(2) $(3) $(4) $(5)
	$$(XDFTOOL_BIN) $$(@:.hdf=-tmp.hdf) delete Prefs/Env-Archive/Sys/ScreenMode.prefs + write build/ScreenMode-$(1).prefs Prefs/Env-Archive/Sys/ScreenMode.prefs
	$(if $(6),$$(PYTHON) tools/make_font_prefs.py build/Font-$(1).prefs topaz.font 8)
	$(if $(6),$$(XDFTOOL_BIN) $$(@:.hdf=-tmp.hdf) delete Prefs/Env-Archive/Sys/Font.prefs + write build/Font-$(1).prefs Prefs/Env-Archive/Sys/Font.prefs)
	$$(XDFTOOL_BIN) $$(@:.hdf=-tmp.hdf) read Prefs/Env-Archive/Sys/ScreenMode.prefs build/ScreenMode-$(1).check
	@cmp -s build/ScreenMode-$(1).prefs build/ScreenMode-$(1).check || { echo "autotest disk: $(1)'s screen mode did not land"; rm -f $$(@:.hdf=-tmp.hdf); exit 1; }
	mv $$(@:.hdf=-tmp.hdf) $$@
endef
$(eval $(call VARIANT_HDF,pal,00029000,640,256,4,1))
$(eval $(call VARIANT_HDF,pal4,00029000,640,256,2,1))
$(eval $(call VARIANT_HDF,aga32,00029000,640,256,5,1))
$(eval $(call VARIANT_HDF,aga256,00029000,640,256,8,1))
$(eval $(call VARIANT_HDF,rtg32,50031303,640,480,24,))

# The helpers the tests drive the machine with (GCC, -lamiga; Grab reads
# RTG screens through Picasso96API, headers only, optional at run time):
#   InputPoke  keys and clicks into input.device   Grab  a screen area as PPM
#   ScreenInfo the front screen's size and font
define TEST_TOOL
dist/Test/$(1): test/tools/$(2).c build/gcc/.flags
	@mkdir -p $$(@D)
	@echo "gcc      $$@"
	$$(Q)$$(GCC_CC) $$(GCC_PROG_CFLAGS) -I$$(P96_INC) $$(GCC_PROG_LDFLAGS) -s $$< -o $$@ -lamiga
endef
$(eval $(call TEST_TOOL,InputPoke,inputpoke))
$(eval $(call TEST_TOOL,Grab,grab))
$(eval $(call TEST_TOOL,ScreenInfo,screeninfo))

# test/rexx: the autotests' helper scripts, beside the tools
AUTO_HELPERS := $(notdir $(wildcard test/rexx/*))
dist/Test/%: test/rexx/%
	@mkdir -p $(@D)
	@cp $< $@

# WaveSmith's single (EditTrim and the others): its side, 40 s, a LIST
# chunk after it
dist/Test/.editsingle: tools/make_edit_single.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/make_edit_single.py $(@D)/Edit
	@touch $@

# a worn record side, three songs (EditMarks: Split at markers)
dist/Test/.vinyl: tools/make_vinyl_side.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/make_vinyl_side.py $(@D)/Split side
	@touch $@

.PHONY: autotest autotest-hdf test-tools
test-tools: dist/Test/InputPoke dist/Test/Grab dist/Test/ScreenInfo $(addprefix dist/Test/,$(AUTO_HELPERS)) \
            dist/Test/.editsingle dist/Test/.vinyl

autotest-hdf: $(AUTOTEST_HDF)

W ?=
AUTOTEST_VARIANT := $(if $(W),-$(W))
autotest: all release-drawer test-tools $(AUTOTEST_HDF) $(if $(W),$(AUTOTEST_HDF_$(W))) fsuae-config-autotest$(AUTOTEST_VARIANT)
	@test -n "$(T)" || (echo "usage: gmake autotest T=<test/auto name> [W=pal|pal4|aga32|aga256|rtg32] [LIMIT=secs]" && exit 1)
	tools/autotest.sh $(abspath $(FSUAE_CONFIG-autotest$(AUTOTEST_VARIANT))) $(T) $(LIMIT)
	@# a test whose results are checked on the host: test/auto/<T>.check
	@if [ -f test/auto/$(T).check ]; then sh test/auto/$(T).check; fi
