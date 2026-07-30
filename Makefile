SHELL := /bin/bash
FRAMEWORKS := onnx openvino pytorch qnn tensorrt tflite
SHELLCHECK := shellcheck

.PHONY: all run-attack test lint clean check-shellcheck check-scripts check-readmes check-makefiles

all: run-attack

run-attack: run_all.sh
	bash ./run_all.sh

lint: check-shellcheck check-scripts check-readmes

test: lint check-makefiles

clean: cleanup_generated.sh
	bash ./cleanup_generated.sh

check-shellcheck:
	@command -v $(SHELLCHECK) >/dev/null 2>&1 || { echo "$(SHELLCHECK) is required for check-shellcheck"; exit 1; }
	@find . -name '*.sh' -print0 | xargs -0 $(SHELLCHECK)

check-scripts:
	@bash -n ./run_all.sh
	@bash -n ./cleanup_generated.sh

check-readmes:
	@for d in $(FRAMEWORKS); do \
		if [ ! -f $$d/README.md ]; then echo "Missing $$d/README.md"; exit 1; fi; \
		for h in "## Overview" "## Structure" "## Requirements" "## Build & run" "## What each target does" "## Troubleshooting / Quirks" "## References"; do \
			if ! grep -q "$$h" "$$d/README.md"; then echo "$$d/README.md missing heading: $$h"; exit 1; fi; \
		done; \
	done

check-makefiles:
	@for d in $(FRAMEWORKS); do \
		if [ ! -f $$d/Makefile ]; then echo "Missing $$d/Makefile"; exit 1; fi; \
		(cd $$d && make -n -s all >/dev/null 2>&1); \
	done
