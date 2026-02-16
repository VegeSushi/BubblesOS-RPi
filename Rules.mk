ifeq ($(strip $(CIRCLEHOME)),)
CIRCLEHOME = ./circle
endif

include $(CIRCLEHOME)/Rules.mk