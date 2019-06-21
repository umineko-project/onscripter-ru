OUTDIR ?= onscrlib
BUILDPKG=./build.sh

# Ideally, this makefile will be replaced with a better
# dependency resolution system, using info in the .pkgbuild
# files themselves.

PROGRAMS=freetype-config libpng14-config libpng-config sdl-config smpeg-config

ARCFILES = $(patsubst %,$(OUTDIR)/bin/%,$(PROGRAMS)) \
	$(OUTDIR)/include \
	$(OUTDIR)/lib

BNAME=onscrlib-$(shell uname)

.PHONY: all
all:
	$(BUILDPKG) $(BUILDOPTS) onscrlib

.PHONY: zip bz2 gz
zip: $(BNAME).zip
bz2: $(BNAME).tar.bz2
gz: $(BNAME).tar.gz

$(BNAME).zip: all
	rm -f $@
	zip -rq $@ $(ARCFILES)

$(BNAME).tar.bz2: all
	rm -f $@
	tar -cjf $@ $(ARCFILES)

$(BNAME).tar.gz: all
	rm -f $@
	tar -czf $@ $(ARCFILES)

.PHONY: clean
clean:
	rm -rf onscrlib onscrlib.* log src
	@if [ -d $(OUTDIR) ]; then \
		echo "Cleaned onscrlib, except for output folder $(OUTDIR)"; fi
