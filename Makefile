SHELL := /bin/sh
BASH_BIN ?= bash
PYTHON_BIN ?= python3

.PHONY: help check test lint

help:
	@printf '%s\n' 'make check : syntaxe Bash' 'make test : tests CLI et intégration (Python 3)' 'make lint : ShellCheck' './rvsh --help : aide'

check:
	@for file in rvsh lib/*.sh tests/*.sh; do $(BASH_BIN) -n "$$file" || exit 1; done

test: check
	$(BASH_BIN) tests/cli.sh
	$(PYTHON_BIN) tests/integration.py

lint:
	shellcheck -x -a rvsh tests/cli.sh
