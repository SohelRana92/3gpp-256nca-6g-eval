# R4 - verify all three 3GPP 256-bit algorithm sets in one command.
#
#   make verify DOCS=path/to/folder
#
# DOCS must hold your copies of the 3GPP documents (Release 20 versions):
#   35243-k00.docx 35244-k00.docx   (AES-256 set)
#   35240-k10.docx 35241-k00.docx   (Snow 5G set)
#   35246-k00.docx 35247-k00.docx   (ZUC-256 set)
# Needs gcc, g++, python3, pandoc, and the Python `cryptography` package
# (any version; AES-GCM-SIV is also checked when the version is 42 or newer).
# Paths with spaces are fine: make verify DOCS="../my docs"

DOCS ?= ../docs
D    := $(shell cd "$(DOCS)" 2>/dev/null && pwd)
FILES = 35243-k00 35244-k00 35240-k10 35241-k00 35246-k00 35247-k00

verify: docs-present
	$(MAKE) -C common check
	$(MAKE) -C aes256set official   TESTDATA="$(D)/35244-k00.docx"
	$(MAKE) -C aes256set crosscheck SPEC="$(D)/35243-k00.docx"
	$(MAKE) -C snow5gset official   TESTDATA="$(D)/35241-k00.docx"
	$(MAKE) -C snow5gset crosscheck SPEC="$(D)/35240-k10.docx"
	$(MAKE) -C zuc256set official   TESTDATA="$(D)/35247-k00.docx"
	$(MAKE) -C zuc256set crosscheck SPEC="$(D)/35246-k00.docx"
	@if [ "$$(uname -m)" = x86_64 ]; then $(MAKE) -C l2x86 check && $(MAKE) -C l3x86 check || exit 1; fi
	@if [ "$$(uname -m)" = aarch64 ]; then $(MAKE) -C l2arm check || exit 1; fi
	@echo "ALL R4 CORRECTNESS CHECKS PASSED"

docs-present:
	@command -v pandoc >/dev/null || { echo "pandoc not found: sudo apt install pandoc"; exit 1; }
	@test -n "$(D)" || { echo "Folder '$(DOCS)' not found. Run: make verify DOCS=path/to/folder"; exit 1; }
	@for f in $(FILES); do test -f "$(D)/$$f.docx" || { echo "Missing $(D)/$$f.docx"; exit 1; }; done

clean:
	$(MAKE) -C common clean
	$(MAKE) -C aes256set clean
	$(MAKE) -C snow5gset clean
	$(MAKE) -C zuc256set clean
	$(MAKE) -C l2x86 clean
	$(MAKE) -C l3x86 clean
	$(MAKE) -C l2arm clean
	$(MAKE) -C bench clean

.PHONY: verify docs-present clean
