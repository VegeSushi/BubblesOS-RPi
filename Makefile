CIRCLEHOME = ./circle

OBJS = main.o kernel.o ubasic/ubasic.o ubasic/tokenizer.o

INCLUDE += -I ./ubasic

LIBS = $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
       $(CIRCLEHOME)/lib/libcircle.a


include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)