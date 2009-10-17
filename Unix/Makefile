ifeq ($(origin SOLUTION_DIR), undefined)
	SOLUTION_DIR := $(shell pwd)/
endif
PROJECT_DIR_NDSECORE := $(dir $(lastword $(MAKEFILE_LIST)))
include $(PROJECT_DIR_NDSECORE)/../distorm64/Unix/Makefile
include $(PROJECT_DIR_NDSECORE)/../libdwarf/Unix/Makefile
clean::

NDSE-Core : libdistorm libdwarf
	@echo Core

