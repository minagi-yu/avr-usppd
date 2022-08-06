DEVICE = avr32da28
ATPACK = /home/minagi/Downloads/AVR-Dx_DFP

DEFS = F_CPU=24000000

COMPORT = /dev/ttyS4

PROJECT := $(shell basename `pwd`)

SRCDIR = src
OBJDIR = obj

CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size

SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))
PROJECT := $(OBJDIR)/$(PROJECT)

CFLAGS += -std=gnu99
CFLAGS += -mmcu=$(DEVICE)
CFLAGS += -Os -mcall-prologues
CFLAGS += -Wall -Wextra
CFLAGS += -B$(addsuffix /gcc/dev/$(DEVICE)/,$(ATPACK))
CFLAGS += -I$(addsuffix /include/,$(ATPACK))
CFLAGS += $(addprefix -D,$(DEFS))
CFLAGS += -Wp,-MM,-MP,-MT,$(OBJDIR)/$(*F).o,-MF,$(OBJDIR)/$(*F).d

LDFLAGS += -Wl,-u,vfprintf -lprintf_min

all: elf hex size

hex: $(PROJECT).hex
elf: $(PROJECT).elf

%.hex: %.elf
	$(OBJCOPY) -j .text -j .rodata -j .data -j .eeprom -j .fuse -O ihex $< $@

size:
	$(SIZE) $(PROJECT).elf

%.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(OBJS) : $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -fr $(OBJDIR)

ping:
	@pymcuprog ping -d $(DEVICE) -t uart -u $(COMPORT)

program:
	@pymcuprog erase -d $(DEVICE) -t uart -u $(COMPORT)
	@pymcuprog write -f $(PROJECT).hex -d $(DEVICE) -t uart -u $(COMPORT)

-include $(shell mkdir -p $(OBJDIR)) $(wildcard $(OBJDIR)/*.d)
