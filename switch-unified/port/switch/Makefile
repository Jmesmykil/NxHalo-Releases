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
ifeq ($(DEKO3D),1)
TARGET := halo-deko
BUILD := build-deko
else
TARGET := halo
BUILD := build
endif
SOURCES		:=	host ../third_party/miniupnpc/src
INCLUDES	:=	include ../android/include ../linux/src ../third_party/miniupnpc/include ../third_party/miniupnpc/src
ifeq ($(DEKO3D),1)
ROMFS := romfs-deko
else
ROMFS := romfs
endif
APP_JSON	:=	$(CURDIR)/halo.json

# NRO metadata
APP_TITLE	:=	HALO: Combat Evolved
APP_AUTHOR	:=	NxHalo contributors
ifeq ($(DEKO3D),1)
APP_VERSION := 0.2.9.9-dk1
else
APP_VERSION := 0.2.9.9-u36
endif
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

ifeq ($(DEKO3D),1)
CFLAGS += -DHALO_SWITCH_DEKO3D=1 -DHOST_DK_UAM_COMMIT=\"5a5afc2bae8b\"
LIBS_DEKO := -ldeko3d
else
EXCLUDE_DEKO := host_dk.c host_dk_shaders.c host_dk_hud.c host_uam_alloc.c host_deko_bridge.c
endif

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
			-lcurl -lz -lstdc++ $(LIBS_DEKO) -lnx -lm -lpthread

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
CFILES := $(filter-out $(EXCLUDE_DEKO),$(CFILES))

SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(DEKO3D),1)
ifeq ($(strip $(UAM_OBJECT)),)
$(error DEKO3D=1 requires verified isolated UAM_OBJECT; no stub shader compiler allowed)
endif
export DEKO_OBJECTS := $(UAM_OBJECT)
endif

export OFILES_SRC	:=	$(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES		:=	$(OFILES_SRC) $(DEKO_OBJECTS)

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

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp $(TOPDIR)/$(ROMFS)/halo_guest.elf
$(OUTPUT).elf: $(OFILES)

# The guest comes from the ILP32 build; never apply host ELF link rules to it.
$(TOPDIR)/$(ROMFS)/halo_guest.elf:
	@echo "Missing guest ELF: build and copy the ILP32 guest first" >&2
	@exit 1

# NSP packaging uses installer/installer.py with explicit local inputs.
# It is separate from compilation, so host builds need no keys or map staging.

-include $(DEPENDS)

endif
