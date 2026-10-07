#---------------------------------------------------------------------------------
# Halo CE for Nintendo Switch — NRO Makefile
#
# Based on the devkitPro/switchbrew NRO template.
# Builds the host (64-bit NRO) that loads and runs the guest image
# (ILP32 AArch64 ELF built by the Android guest toolchain).
#
# Usage:
#   make              — build host NRO/NSO/NPDM artifacts
#   make host         — build host artifacts without packaging an NSP
#   make clean        — remove build artifacts
#   make deploy       — send to Switch via nxlink
#
# Prerequisites:
#   - devkitPro with devkitA64, libnx, switch-sdl2, switch-mesa
#   - halo_guest.elf in romfs/ (built by the Android guest pipeline)
#   - Player NSP packaging is explicit: see installer/README.md
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Build configuration
#---------------------------------------------------------------------------------
TARGET		:=	halo
BUILD		:=	build
SOURCES		:=	host ../third_party/miniupnpc/src
INCLUDES	:=	include ../android/include ../linux/src ../third_party/miniupnpc/include ../third_party/miniupnpc/src
ROMFS		:=	romfs
ROMFS_FILES	:=	$(wildcard $(TOPDIR)/$(ROMFS)/*)
APP_JSON	:=	$(CURDIR)/halo.json

# NRO metadata
APP_TITLE	:=	HALO: Combat Evolved
APP_AUTHOR	:=	NxHalo contributors
APP_VERSION	:=	0.1.11-p33
APP_TITLEID	:=	010048414C210000
export NACPFLAGS += --titleid=$(APP_TITLEID)

#---------------------------------------------------------------------------------
# Compiler / linker flags
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES) \
			-D__SWITCH__=1 \
			-DHALO_SWITCH=1 -DMINIUPNP_STATICLIB -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_DEFAULT_SOURCE \
			-DENABLE_NXLINK=1

CFLAGS	+=	$(INCLUDE) \
			-I$(DEVKITPRO)/portlibs/switch/include \
			-I$(DEVKITPRO)/portlibs/switch/include/SDL2

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)

LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) \
			-Wl,-Map,$(notdir $*.map) -Wl,--gc-sections -Wl,--wrap=svcSendSyncRequest -Wl,--wrap=fatalThrow

export LD		:=	$(CXX)
export LIBPATHS	:=	-L$(DEVKITPRO)/portlibs/switch/lib -L$(DEVKITPRO)/libnx/lib

#---------------------------------------------------------------------------------
# Libraries
#
# Order matters: SDL2 and GL must come before system libs.
# switch-mesa provides OpenGL ES via the Tegra X1's native GL.
#---------------------------------------------------------------------------------
LIBS	:=	-lSDL2 \
			-lGLESv2 -lEGL \
			-lglapi -ldrm_nouveau \
			-lcurl -lz -lstdc++ -lnx -lm -lpthread

#---------------------------------------------------------------------------------
# Source file discovery
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export APP_JSON	:=	$(CURDIR)/halo.json
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES_SRC	:=	$(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES		:=	$(OFILES_SRC)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD) \
					-I$(DEVKITPRO)/libnx/include

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		else
			export APP_ICON := $(DEVKITPRO)/libnx/default_icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

export NROFLAGS	+=	--icon=$(APP_ICON)
export NROFLAGS	+=	--nacp=$(CURDIR)/$(TARGET).nacp

ifneq ($(strip $(ROMFS)),)
	export NROFLAGS	+=	--romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) host clean deploy

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# Keep host iteration independent of map staging and NSP signing keys.
host:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).nro $(OUTPUT).nso $(OUTPUT).npdm

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).pfs0 $(TARGET).nsp $(TARGET).nso $(TARGET).npdm exefs

deploy: $(BUILD)
	@echo "Deploying $(TARGET).nro via nxlink..."
	nxlink -s $(OUTPUT).nro

else

DEPENDS	:=	$(OFILES:.o=.d)

.PHONY: all

all: $(OUTPUT).nro $(OUTPUT).nso $(OUTPUT).npdm

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp $(ROMFS_FILES)
$(OUTPUT).elf: $(OFILES)

# NSP packaging uses installer/installer.py with explicit local inputs.
# It is separate from compilation, so host builds need no keys or map staging.

-include $(DEPENDS)

endif
