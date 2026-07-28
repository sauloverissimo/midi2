# atmega32u4-device-baremetal
# USB MIDI 2.0 device on the ATmega32U4 (Arduino Pro Micro): the midi2 C99
# core over the midi2lufa transport, bare metal.
#
# Requires LUFA-210130 and a midi2lufa checkout:
#   git clone --branch LUFA-210130 --depth 1 https://github.com/abcminiuser/lufa
#   git clone https://github.com/sauloverissimo/midi2lufa
#   make LUFA_PATH=/path/to/lufa/LUFA MIDI2LUFA_PATH=/path/to/midi2lufa

MCU          = atmega32u4
ARCH         = AVR8
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = atmega32u4-midi2
SRC          = src/main.c src/stream_responder.c src/ci_responder.c src/catalog.c \
               $(MIDI2LUFA_PATH)/src/midi2lufa.c \
               $(MIDI2LUFA_PATH)/src/midi2lufa_descriptors.c \
               ../../src/midi2_dispatch.c ../../src/midi2_proc.c ../../src/midi2_ci.c \
               $(LUFA_SRC_USB_DEVICE)
LUFA_PATH      ?= ../lufa/LUFA
MIDI2LUFA_PATH ?= ../../../midi2lufa

# Board selection: promicro (default) or leonardo. Controls LED mapping and
# the USB product string.
BOARD_SEL   ?= promicro

CC_FLAGS  = -DUSE_LUFA_CONFIG_HEADER -Isrc -I../../src -I$(MIDI2LUFA_PATH)/src
CC_FLAGS += -DBOARD_SEL_$(shell echo $(BOARD_SEL) | tr '[:lower:]' '[:upper:]')
CC_FLAGS += -DMIDI2LUFA_VID=0xCAFE -DMIDI2LUFA_PID=0x40D0
CC_FLAGS += '-DMIDI2LUFA_MANUFACTURER=L"midi2.diy"'
PRODUCT_STR = $(if $(filter leonardo,$(BOARD_SEL)),midi2 Leonardo,midi2 Pro Micro)
CC_FLAGS += '-DMIDI2LUFA_PRODUCT=L"$(PRODUCT_STR)"'
CC_FLAGS += -ffunction-sections -fdata-sections -flto
LD_FLAGS  = -Wl,--gc-sections -flto

# avrdude: Caterina bootloader (enter with a 1200 bps touch on the CDC port)
AVRDUDE_PROGRAMMER = avr109
AVRDUDE_PORT      ?= /dev/ttyACM0

include $(LUFA_PATH)/Build/LUFA/lufa-sources.mk
include $(LUFA_PATH)/Build/LUFA/lufa-gcc.mk
include $(LUFA_PATH)/Build/DMBS/DMBS/core.mk
include $(LUFA_PATH)/Build/DMBS/DMBS/gcc.mk
include $(LUFA_PATH)/Build/DMBS/DMBS/avrdude.mk
