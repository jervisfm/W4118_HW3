# Set this to the name of your program
TARGET = sensorsim

# Edit this variable to point to all
# of your sources files (make sure
# to put a complete relative path)
SOURCES = sensorsim.c

CRUNTIME = crtbegin_dynamic.S \
           crtend.S

# -----------------------------------------------
#
# Don't touch things below here unless
# you know what you're doing :-)
#
OBJECTS = $(SOURCES:%.c=%.c.o) $(CRUNTIME:%.S=%.S.o)
INCLUDE = -I. -I../inc
EXTRA_LIBS = -lc -lhardware
CFLAGS = -O3 -fomit-frame-pointer -Wall $(INCLUDE) $(EXTRA_CFLAGS)
LDFLAGS = --entry=_start --dynamic-linker /system/bin/linker \
          -nostdlib -rpath /system/lib -rpath ../system/lib \
          -L ../system/lib $(EXTRA_LIBS)
CC = arm-none-linux-gnueabi-gcc
LD = arm-none-linux-gnueabi-ld

default: $(SOURCES) $(TARGET)

$(TARGET): $(OBJECTS)
	@echo [ARM-LD] $@
	@$(LD) $(LDFLAGS) $(OBJECTS) -o $@

%.c.o: %.c
	@echo [ARM-CC] $<...
	@$(CC) -c $(CFLAGS) $< -o $@

%.S.o: %.S
	@echo [ARM-ASM] $<...
	@$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	@echo [CLEAN] removing object files + $(TARGET)...
	@rm -f $(OBJECTS) $(TARGET)

%_emu: ADB_OPT = -e
%_emu: ADB_TGT = Android Emulator
%_dev: ADB_OPT = -d
%_dev: ADB_TGT = Android Device

install_emu: install
install_dev: install
install: $(TARGET)
	@echo [INSTALL] moving $(TARGET) to $(ADB_TGT)...
	@adb $(ADB_OPT) remount
	@adb $(ADB_OPT) push $(TARGET) /system/bin
	@adb $(ADB_OPT) shell chmod 6755 /system/bin/$(TARGET)

run_emu: run
run_dev: run
run: install
	@echo [RUN] calling /system/bin/$(TARGET) on $(ADB_TGT)...
	@adb $(ADB_OPT) shell /system/bin/$(TARGET) $(RUN_OPTS)

.PHONY:

