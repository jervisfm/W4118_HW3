# Set this to the name of your program
TARGET = orientd

TARGET_DIR = /data/misc

# Edit this variable to point to all
# of your sources files (make sure
# to put a complete relative path)
SOURCES = orientd.c

CRUNTIME = crtbegin_dynamic.S \
           crtend.S

# -----------------------------------------------
# 
# Don't touch things below here unless
# you know what you're doing :-)
# 
OBJECTS = $(SOURCES:%.c=%.c.o) $(CRUNTIME:%.S=%.S.o)
INCLUDE = -I. -I./inc -I..android-tegra-3.1/include -I../android-tegra-3.1/arch/arm/include
EXTRA_LIBS = -lc -lhardware
CFLAGS = -g -O2 -Wall $(INCLUDE) $(EXTRA_CFLAGS)
LDFLAGS = --entry=_start --dynamic-linker /system/bin/linker \
          -nostdlib -rpath /system/lib -rpath ./system/lib \
          -L ./system/lib $(EXTRA_LIBS)
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

install_%: $(TARGET)
	@echo [INSTALL] moving $(TARGET) to $(ADB_TGT) [$(TARGET_DIR)]...
	@adb $(ADB_OPT) remount
	@adb $(ADB_OPT) push $(TARGET) $(TARGET_DIR)
	@adb $(ADB_OPT) shell chmod 6755 $(TARGET_DIR)/$(TARGET)

user_install_%: $(TARGET)
	@echo [INSTALL:user] moving $(TARGET) to $(ADB_TGT) [$(TARGET_DIR)]...
	@adb $(ADB_OPT) remount
	@adb $(ADB_OPT) push $(TARGET) $(TARGET_DIR)
	@adb $(ADB_OPT) shell chmod 0750 $(TARGET_DIR)/$(TARGET)
	@adb $(ADB_OPT) shell chown compass.system $(TARGET_DIR)/$(TARGET)

run_%: $(TARGET)
	@echo [RUN] calling $(TARGET_DIR)/$(TARGET) on $(ADB_TGT)...
	@adb $(ADB_OPT) shell stop akmd
	@adb $(ADB_OPT) shell start akmd
	@echo 
	@echo "You may have to keep your screen off for this to work..."
	@echo 
	@sleep 2
	@echo "$(TARGET_DIR)/$(TARGET) && exit" | adb $(ADB_OPT) shell

.PHONY:

