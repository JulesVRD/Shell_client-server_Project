SHELL := /bin/sh
BASH_BIN ?= bash

.PHONY: help check test

help:
	@printf '%s\n' 'make check : vérifier la syntaxe Bash' 'make test  : vérifier la syntaxe et les arguments de rvsh' './rvsh --help : afficher les modes attendus'

check:
	$(BASH_BIN) -n rvsh tests/cli.sh

test: check
	$(BASH_BIN) tests/cli.sh
