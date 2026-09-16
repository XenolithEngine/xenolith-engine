# Sample makefile for xlmake: nested includes.
# Inspect with:  xlmake -f include-main.mk -V INSTALL_DIR -V TAG

include include-config.mk

INSTALL_DIR := $(PREFIX)/bin
TAG         := release-$(VERSION)
