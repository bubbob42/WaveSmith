# --- the release package -------------------------------------------------------------
# (included by GNUmakefile)
#
# One drawer, WaveSmith, and its icon -- the archive Aminet gets:
#
#   WaveSmith/Install, .info           the Installer script (install/Install,
#                                      its comments stripped)
#   WaveSmith/WaveSmith.readme, .info  the Aminet readme, readable before
#                                      anything is installed
#   WaveSmith/LICENSE, COPYING.LIB     what the LGPL covers, and its text
#                                      (with the readme's text icon)
#   WaveSmith/Files/WaveSmith/         the drawer the Installer copies whole:
#                                      the program (the SAS/C build) and its
#                                      icon, Help/<language>/WaveSmith.guide,
#                                      Catalogs/<language>/WaveSmith.catalog
#   WaveSmith/Files/WaveSmith.info     that drawer's classic icon
#   WaveSmith/Files/Icons/Glow/        the GlowIcon set, at the user's
#                                      choice: WaveSmithDrawer.info (Mason's
#                                      drawer), and WaveSmith/ -- a mirror of
#                                      the icons in Files/WaveSmith (Mason's
#                                      program icon, AmigaOS 3.2's drawer and
#                                      AmigaGuide GlowIcons for Help), which
#                                      the Installer copies over them whole
#
# The package's own drawer wears Mason's drawer art (a superset: classic
# Workbenches see ours). Every drawer is laid out by tools/arrange_drawers.py
# (its rules are written there): the package's face, and everything the
# Installer copies, so an installed WaveSmith opens arranged; the icons that
# land in the user's own drawers (SYS:Tools, wherever the archive is
# unpacked) have no place -- Workbench finds one.
#
#   gmake release-stage    build/release/WaveSmith, checked
#   gmake release-drawer   the same in dist/Release (DEV:Release in FS-UAE)
#   gmake release-archive  dist/WaveSmith.lha (Amiga LhA under vamos)
#   gmake release-aminet   dist/aminet: the archive and its readme side by
#                          side, checked -- NOT uploaded
#   gmake release-source   build/public/WaveSmith: the public source of
#                          REF (default the tag of WaveSmith_rev.h's
#                          version), as github.com/bubbob42/WaveSmith gets
#                          it -- NOT pushed (tools/public_source.sh)
RELSTAGE   := build/release
RELNAME    := WaveSmith
RELDRAWER  := $(RELSTAGE)/$(RELNAME)
RELFILES   := $(RELDRAWER)/Files/WaveSmith
RELGLOW    := $(RELDRAWER)/Files/Icons/Glow
AMINETDIR  := dist/aminet
AMIGA_LHA  ?= $(HOME)/Developer/AmigaTools/Lha_40/Lha
# LhA's pools outgrow a 68000's 16 MB: a 68020 with 128 MB; `--` keeps
# vamos from eating LhA's -r
VAMOS_LHA   = $(VAMOS_LOCK) $(VAMOS) -c $(CURDIR)/$(VAMOSRC) -q -H disable -C 68020 -m 131072 --
README_SRC := install/WaveSmith.readme.in tools/make_aminet_readme.py WaveSmith/WaveSmith_rev.h
LICENCES   := LICENSE COPYING.LIB

build/Install: install/Install tools/check_installer.py
	@mkdir -p $(@D)
	$(Q)$(PYTHON) tools/check_installer.py install/Install
	$(Q)$(PYTHON) tools/check_installer.py --strip install/Install > $@

.PHONY: installer release-stage release-drawer release-archive release-aminet release-source
installer: build/Install

release-stage: dist/sasc/WaveSmith build/Install build/edit-icons/.stamp $(WS_GLOW) $(WS_DRAWER_GLOW) $(WS_INSTALL_GLOW) $(WS_README_GLOW) \
               $(WS_FOLDER_GLOW) $(WS_GUIDE_GLOW) $(README_SRC) $(LICENCES) $(WS_GUIDES) $(WS_CATALOGS)
	@rm -rf $(RELSTAGE)
	@mkdir -p $(RELFILES) $(RELGLOW)
	cp build/Install $(RELDRAWER)/Install
	@# AmigaOS 3.2's standard installer icon, classic and GlowIcon (GNUmakefile)
	cp $(WS_INSTALL_GLOW) $(RELDRAWER)/Install.info
	$(Q)$(PYTHON) tools/make_aminet_readme.py --template install/WaveSmith.readme.in --rev WaveSmith/WaveSmith_rev.h \
	    --name $(RELNAME) -o $(RELDRAWER)/$(RELNAME).readme >/dev/null
	$(Q)$(PYTHON) tools/make_aminet_readme.py --check $(RELDRAWER)/$(RELNAME).readme
	@# the readme's: our text icon and Mason's About GlowIcon (GNUmakefile)
	cp $(WS_README_GLOW) $(RELDRAWER)/$(RELNAME).readme.info
	@# the licence: what it covers, and the LGPL's text, with the readme's
	@# icon (a text for MultiView)
	cp $(LICENCES) $(RELDRAWER)/
	@for f in $(LICENCES); do cp $(WS_README_GLOW) $(RELDRAWER)/$$f.info || exit 1; done
	@# the drawer the Installer copies: the program, the manuals, the catalogs
	cp dist/sasc/WaveSmith build/edit-icons/WaveSmith.info $(RELFILES)/
	@for g in $(WS_GUIDES); do \
		l=`basename \`dirname $$g\``; \
		mkdir -p $(RELFILES)/Help/$$l; \
		cp $$g $(RELFILES)/Help/$$l/ || exit 1; \
		cp build/edit-icons/Guide.info $(RELFILES)/Help/$$l/WaveSmith.guide.info; \
		cp build/edit-icons/Folder.info $(RELFILES)/Help/$$l.info; \
		mkdir -p $(RELGLOW)/WaveSmith/Help/$$l; \
		cp $(WS_GUIDE_GLOW) $(RELGLOW)/WaveSmith/Help/$$l/WaveSmith.guide.info; \
		cp $(WS_FOLDER_GLOW) $(RELGLOW)/WaveSmith/Help/$$l.info; \
	done
	cp build/edit-icons/Folder.info $(RELFILES)/Help.info
	cp $(WS_FOLDER_GLOW) $(RELGLOW)/WaveSmith/Help.info
	@for c in $(WS_CATALOGS); do \
		l=`basename \`dirname $$c\``; \
		mkdir -p $(RELFILES)/Catalogs/$$l; \
		cp $$c $(RELFILES)/Catalogs/$$l/ || exit 1; \
	done
	cp build/edit-icons/Drawer.info $(RELDRAWER)/Files/WaveSmith.info
	@# the GlowIcons, for the Installer to copy when they are chosen
	cp $(WS_GLOW) $(RELGLOW)/WaveSmith/WaveSmith.info
	cp $(WS_DRAWER_GLOW) $(RELGLOW)/WaveSmithDrawer.info
	@find $(RELSTAGE) \( -name '._*' -o -name '.DS_Store' -o -name '*.uaem' \) -delete
	@# LhA records the HOST file mode as protection bits: nothing read-only
	chmod -R u+w $(RELSTAGE)
	@# the package's own drawer in Mason's drawer art
	cp $(WS_DRAWER_GLOW) $(RELSTAGE)/$(RELNAME).info
	@# every drawer laid out: the package's face and everything the Installer
	@# copies (positions, windows, "show only icons"), checked
	$(Q)$(PYTHON) tools/arrange_drawers.py $(RELSTAGE) >/dev/null
	@ok=1; \
	for f in Install Install.info $(RELNAME).readme $(RELNAME).readme.info $(LICENCES) $(addsuffix .info,$(LICENCES)) \
	         Files/WaveSmith.info Files/WaveSmith/WaveSmith Files/WaveSmith/WaveSmith.info Files/WaveSmith/Help.info \
	         Files/Icons/Glow/WaveSmith/WaveSmith.info Files/Icons/Glow/WaveSmith/Help.info \
	         Files/Icons/Glow/WaveSmithDrawer.info \
	         $(patsubst dist/Help/%,Files/WaveSmith/Help/%,$(WS_GUIDES)) \
	         $(patsubst dist/Catalogs/%,Files/WaveSmith/Catalogs/%,$(WS_CATALOGS)); do \
		test -s "$(RELDRAWER)/$$f" || { echo "  MISSING $$f"; ok=0; }; \
	done; \
	cmp -s dist/sasc/WaveSmith $(RELFILES)/WaveSmith || { echo "  Files/WaveSmith/WaveSmith is not this build"; ok=0; }; \
	grep -q '^;' $(RELDRAWER)/Install && { echo "  Install still carries comments"; ok=0; }; \
	v=`sed -n 's/^#define VERS[[:space:]]*"WaveSmith \(.*\)"/\1/p' WaveSmith/WaveSmith_rev.h`; \
	grep -q "^Version:      $$v$$" $(RELDRAWER)/$(RELNAME).readme || { echo "  the readme is not for $$v"; ok=0; }; \
	strings $(RELFILES)/WaveSmith | grep -q "WaveSmith $$v " || { echo "  the program's \$$VER is not $$v"; ok=0; }; \
	test $$ok -eq 1 || { echo "release-stage: incomplete"; exit 1; }
	@echo "release-stage: $(RELDRAWER)"

release-drawer: release-stage
	@rm -rf dist/Release
	@mkdir -p dist/Release
	cp -R $(RELSTAGE)/. dist/Release/
	cp build/edit-icons/Folder.info dist/Release.info
	@find dist/Release \( -name '._*' -o -name '.DS_Store' \) -delete

# ONE drawer and its icon
release-archive: release-stage | $(VAMOSRC)
	@test -x "$(VAMOS)" -a -f "$(AMIGA_LHA)" || { echo "release-archive: needs vamos and $(AMIGA_LHA)"; exit 1; }
	@extra=`ls $(RELSTAGE) | grep -v -x -e '$(RELNAME)' -e '$(RELNAME).info'`; \
	 test -z "$$extra" || { echo "release-archive: $(RELSTAGE) holds more than the drawer and its icon:"; echo "$$extra"; exit 1; }
	@rm -f dist/$(RELNAME).lha
	$(Q)cd $(RELSTAGE) && $(VAMOS_LHA) $(AMIGA_LHA) -r a proj:dist/$(RELNAME).lha "#?" >/dev/null
	$(Q)$(VAMOS_LHA) $(AMIGA_LHA) v proj:dist/$(RELNAME).lha > build/release-list.txt 2>&1 || true
	$(Q)$(PYTHON) tools/check_lha_bits.py dist/$(RELNAME).lha `find $(RELSTAGE) -type f | wc -l`
	@ok=1; \
	for f in $(RELNAME)/Install $(RELNAME)/$(RELNAME).readme $(addprefix $(RELNAME)/,$(LICENCES)) $(RELNAME)/Files/WaveSmith/WaveSmith \
	         $(RELNAME)/Files/Icons/Glow/WaveSmith/WaveSmith.info; do \
		grep -q "$$f" build/release-list.txt || { echo "  MISSING in the archive: $$f"; ok=0; }; \
	done; \
	if grep -iE '\.prefs([^a-z]|$$)' build/release-list.txt; then echo "  private settings in the archive"; ok=0; fi; \
	test $$ok -eq 1 || { echo "release-archive: see build/release-list.txt"; exit 1; }
	@echo "release-archive: dist/$(RELNAME).lha"
	@ls -l dist/$(RELNAME).lha

# Aminet takes a PAIR: the archive and a readme of the same base name beside
# it. Staged, checked, NOT uploaded.
release-aminet: release-archive
	@rm -rf $(AMINETDIR)
	@mkdir -p $(AMINETDIR)
	cp dist/$(RELNAME).lha $(AMINETDIR)/$(RELNAME).lha
	@# the very readme the archive carries, so the two cannot differ
	cp $(RELDRAWER)/$(RELNAME).readme $(AMINETDIR)/$(RELNAME).readme
	$(Q)$(PYTHON) tools/make_aminet_readme.py --check $(AMINETDIR)/$(RELNAME).readme
	@if grep -q '^Replaces:' $(AMINETDIR)/$(RELNAME).readme; then \
		echo "release-aminet: an UPDATE -- `grep '^Replaces:' $(AMINETDIR)/$(RELNAME).readme`"; \
	else \
		echo "release-aminet: a FIRST upload (no Replaces:). From the second one on, add"; \
		echo "                \"Replaces: mus/edit/$(RELNAME).lha\" to install/WaveSmith.readme.in"; \
	fi
	@echo "release-aminet: ready in $(AMINETDIR)/ -- NOT uploaded."
	@ls -l $(AMINETDIR)/

# The LGPL's source for the archive: the released commit's files without
# the development notes kept in this repository only (the script names
# them), as the public repository gets them, one commit a release. Built
# from a TAG, so what is published is what was released.
REL_VERS := $(shell sed -n 's/^.define VERS[[:space:]]*"WaveSmith \(.*\)"/\1/p' WaveSmith/WaveSmith_rev.h)
REF      ?= v$(REL_VERS)
release-source:
	@git rev-parse -q --verify "$(REF)^{commit}" >/dev/null || \
	    { echo "release-source: no $(REF) -- tag the release first (or REF=<commit>)"; exit 1; }
	$(Q)sh tools/public_source.sh $(REF) build/public/$(RELNAME)
