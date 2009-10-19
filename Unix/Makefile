ifeq ($(origin SOLUTION_DIR), undefined)
	SOLUTION_DIR := $(shell pwd)/
endif
PROJECT_DIR_NDSECORE := $(dir $(lastword $(MAKEFILE_LIST)))
include $(PROJECT_DIR_NDSECORE)/../distorm64/Unix/Makefile
include $(PROJECT_DIR_NDSECORE)/../libdwarf/Unix/Makefile
clean::
	rm -f libNDSE.so*
	rm -f libNDSE

NDSE-Core : libdistorm libdwarf
	cd $(PROJECT_DIR_NDSECORE)/../Core && make
	cp $(PROJECT_DIR_NDSECORE)/../Core/libNDSE.so* .
	cd $(PROJECT_DIR_NDSECORE)/../Core && make -i clean
	touch libNDSE

