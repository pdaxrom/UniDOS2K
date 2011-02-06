CHIP		 = LPC2478
USE_EMC		 = yes
USE_LCD		 = yes
#USE_KBD		 = yes

#CHIP		 = LPC2378
#USE_EMC		 = no
#USE_LCD		 = no
#USE_KBD		 = no
#USE_LIBFS	 = efsl

TOOLSET		:= /opt/arm-2010.09/bin
TOOLCHAIN	:= arm-none-eabi-

CROSS		:= $(TOOLSET)/$(TOOLCHAIN)

TARGET		:= bin/$(CHIP)_UniDOS.elf
BIN		:= bin/$(CHIP)_UniDOS.bin
HEX		:= bin/$(CHIP)_UniDOS.hex

all: directories $(TARGET) $(BIN) $(HEX)

AS		:= $(CROSS)gcc -c
CC		:= $(CROSS)gcc
CXX		:= $(CROSS)g++
LD		:= $(CROSS)g++
OBJCOPY		:= $(CROSS)objcopy
OBJDUMP		:= $(CROSS)objdump

CFLAGS		 = -Os
CFLAGS		+= -fomit-frame-pointer
CFLAGS		+= -Wall -Wextra -Wundef -Wcast-align
CFLAGS		+= -mcpu=arm7tdmi
CFLAGS		+= -D$(CHIP)
CFLAGS		+= -Iinclude
CFLAGS		+= -Ifullfat/src
CFLAGS		+= $(CFLAGS_OPT)

CXXFLAGS	 = $(CFLAGS)

LDFLAGS		 = -mcpu=arm7tdmi
LDFLAGS		+= -nostartfiles -Xlinker -Tldscripts/$(CHIP)_ROM.ld

LPC21ISP	 = lpc21isp
LPC21ISP_PORT	 = /dev/ttyUSB0
LPC21ISP_BAUD	 = 230400
LPC21ISP_XTAL	 = 12000
LPC21ISP_OPTIONS = -control -verify

STARTUP_OBJS	 = startup/crt.o startup/system.o startup/main.o startup/uart.o startup/fio.o startup/leds.o startup/interrupts.o startup/irq.o \
    startup/mci.o startup/swi.o

ifeq (efsl, $(USE_LIBFS))
LIBFS_OBJS	 = efsl/Src/debug.o efsl/Src/dir.o efsl/Src/disc.o efsl/Src/efs.o efsl/Src/extract.o efsl/Src/fat.o efsl/Src/file.o efsl/Src/fs.o \
    efsl/Src/ioman.o efsl/Src/ls.o efsl/Src/mkfs.o efsl/Src/partition.o efsl/Src/plibc.o efsl/Src/time.o efsl/Src/ui.o efsl/lpc24mci.o
CFLAGS		+= -DUSE_LIBFS_EFSL -Istartup -Iefsl/Inc
else
LIBFS_OBJS	 = fullfat/src/ff_blk.o fullfat/src/ff_crc.o fullfat/src/ff_dir.o fullfat/src/ff_error.o fullfat/src/ff_fat.o fullfat/src/ff_file.o \
    fullfat/src/ff_format.o fullfat/src/ff_hash.o fullfat/src/ff_ioman.o fullfat/src/ff_memory.o fullfat/src/ff_safety.o fullfat/src/ff_string.o \
    fullfat/src/ff_time.o fullfat/src/ff_unicode.o startup/blkdev_mci.o
endif

CMDSHELL_OBJS	 = src/unidos.o src/syscalls.o src/loadelf.o

ifeq (yes, $(USE_LCD))
CFLAGS		+= -DUSE_LCD
STARTUP_OBJS	+= startup/lcd.o startup/console/koi5x8.o startup/console/screen.o startup/console/console.o startup/console/cursor.o
endif

ifeq (yes, $(USE_EMC))
CFLAGS		+= -DUSE_EMC
STARTUP_OBJS	+= startup/emc.o
endif

ifeq (yes, $(USE_KBD))
CFLAGS		+= -DUSE_KBD
STARTUP_OBJS	+= startup/kbd.o
endif

OBJS		 = $(STARTUP_OBJS) $(LIBFS_OBJS) $(CMDSHELL_OBJS)

$(TARGET): $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

$(HEX): $(TARGET)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(TARGET)
	$(OBJCOPY) -O binary $< $@

directories:
	mkdir -p bin

utils/lpc21isp: utils/lpc21isp_148x.c
	gcc $^ -o $@

load program: $(HEX) utils/lpc21isp
	utils/lpc21isp $(LPC21ISP_OPTIONS) $< $(LPC21ISP_PORT) $(LPC21ISP_BAUD) $(LPC21ISP_XTAL)

loadterm: $(HEX) utils/lpc21isp
	utils/lpc21isp $(LPC21ISP_OPTIONS) -term $< $(LPC21ISP_PORT) $(LPC21ISP_BAUD) $(LPC21ISP_XTAL)

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf bin
	rm -f utils/lpc21isp
