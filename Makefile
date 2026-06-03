#---------------------------------------------------------------------------------
# NXSand - Nintendo Switch homebrew (SDL2 + OpenGL falling-sand)
#
# `make`         -> build/NXSand.nro (Switch homebrew, devkitPro toolchain)
# `make desktop` -> build/NXSand (SDL2 + OpenGL ES 3.0 via GLESv2)
# `make clean`   -> remove build artifacts
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# Desktop target (developer iteration). Auto-detects pkg-config for SDL2.
#---------------------------------------------------------------------------------
DESKTOP_CXX      ?= g++
DESKTOP_CXXFLAGS := -std=c++17 -O3 -Wall -Wno-missing-field-initializers \
                    -Isource -Ithird_party -Ithird_party/glad/include -DNX_DESKTOP=1 \
                    $(shell pkg-config --cflags sdl2 glesv2 freetype2 2>/dev/null)
DESKTOP_GLAD_C  := third_party/glad/src/gles2.c
ifneq ($(filter-out 0 false FALSE off OFF no NO,$(strip $(NXSAND_ENABLE_COMPUTE))),)
DESKTOP_CXXFLAGS += -DNXSAND_ENABLE_COMPUTE_DEFAULT=1
endif
DESKTOP_LDFLAGS  :=
DESKTOP_LIBS     := $(shell pkg-config --libs sdl2 glesv2 freetype2 2>/dev/null || echo "-lSDL2 -lGLESv2 -lfreetype")

DESKTOP_SRCS := $(wildcard source/platform/*.cpp) \
                $(wildcard source/platform/audio/*.cpp) \
                $(wildcard source/platform/input/*.cpp) \
                $(wildcard source/game/*.cpp) \
                $(wildcard source/gpu/*.cpp) \
                $(wildcard source/sim/*.cpp) \
                $(wildcard source/ui/*.cpp) \
                $(wildcard source/save/*.cpp)

UNIT_CXXFLAGS := -std=c++17 -O3 -Wall -Isource -Ithird_party -Itests
UNIT_SRCS     := tests/unit_main.cpp \
                 tests/unit_materials.cpp \
                 tests/unit_sim_cpu.cpp \
                 tests/unit_physics.cpp \
                 tests/unit_base64.cpp \
                 tests/unit_physics_gpu.cpp \
                 tests/unit_physics_settings.cpp \
                 tests/unit_layout.cpp \
                 tests/unit_sim_grid.cpp \
                 tests/unit_settings.cpp \
                 tests/unit_brush_stroke.cpp \
                 tests/unit_active_tiles.cpp \
                 tests/unit_tpt_import.cpp \
                 tests/unit_perf_preset_physics.cpp \
                 tests/unit_menu_repeat.cpp \
                 source/save/tpt_stamp_import.cpp \
                 source/game/game_settings.cpp \
                 source/save/settings_io.cpp \
                 source/sim/cpu_reference.cpp \
                 source/sim/physics_settings.cpp \
                 source/sim/brush_stroke.cpp \
                 source/save/physics_params_io.cpp \
                 source/save/save_paths.cpp \
                 source/save/base64.cpp \
                 source/platform/input/menu_repeat.cpp

GPU_UNIT_CXXFLAGS := -std=c++17 -O2 -Wall -Wno-missing-field-initializers \
                     -DNX_DESKTOP=1 -Isource -Ithird_party -Ithird_party/glad/include -Itests \
                     $(shell pkg-config --cflags sdl2 glesv2 2>/dev/null)
GPU_UNIT_GLAD_C   := third_party/glad/src/gles2.c
GPU_UNIT_LIBS     := $(shell pkg-config --libs sdl2 glesv2 2>/dev/null || echo "-lSDL2 -lGLESv2")
GPU_UNIT_SRCS     := tests/gpu_unit_main.cpp \
                     tests/unit_gpu_sim.cpp \
                     tests/gpu_test_gl.cpp \
                     source/gpu/sim_pipeline.cpp \
                     source/gpu/sim_backend.cpp \
                     source/gpu/shader_program.cpp \
                     source/gpu/shader_cache.cpp \
                     source/gpu/gl_loader.cpp \
                     source/save/save_paths.cpp

.PHONY: desktop test test-gpu golden clean help dist nsp
desktop:
	@mkdir -p build
	$(DESKTOP_CXX) $(DESKTOP_CXXFLAGS) $(DESKTOP_SRCS) $(DESKTOP_GLAD_C) -o build/NXSand $(DESKTOP_LDFLAGS) $(DESKTOP_LIBS)

test:
	@mkdir -p build
	$(DESKTOP_CXX) $(UNIT_CXXFLAGS) $(UNIT_SRCS) -o build/unit_tests
	./build/unit_tests

golden: test

test-gpu:
	@mkdir -p build
	$(DESKTOP_CXX) $(GPU_UNIT_CXXFLAGS) $(GPU_UNIT_SRCS) $(GPU_UNIT_GLAD_C) -o build/gpu_unit_tests $(GPU_UNIT_LIBS)
	@if [ "$$(uname -s 2>/dev/null)" = "Linux" ] && command -v xvfb-run >/dev/null 2>&1; then \
		LIBGL_ALWAYS_SOFTWARE=$${LIBGL_ALWAYS_SOFTWARE:-1} xvfb-run -a -s "-screen 0 128x128x24" ./build/gpu_unit_tests; \
	else \
		./build/gpu_unit_tests; \
	fi

#---------------------------------------------------------------------------------
# Switch target. Requires devkitPro env: $DEVKITPRO must be set.
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
.DEFAULT_GOAL := help
help:
	@echo "NXSand build help"
	@echo "  make desktop   -> build/NXSand (SDL2 + GLESv2)"
	@echo "  make test      -> CPU unit tests (no GPU/SDL)"
	@echo "  make test-gpu  -> GLES sim pipeline tests (SDL offscreen + Mesa)"
	@echo "  make golden    -> alias for make test"
	@echo "  make           -> build/NXSand.nro for Nintendo Switch"
	@echo "                    Requires devkitPro shell: (dkp-)pacman -S switch-dev switch-sdl2 switch-mesa switch-glm switch-freetype switch-harfbuzz"
	@echo "                    On Windows: scripts/build-native.ps1; FTP deploy: scripts/serve-nro-ftp.ps1"
	@echo "  make dist      -> copy build/NXSand.nro to dist/switch/ (after Switch make)"
	@echo "  make nsp       -> dist/switch/NXSand.nsp forwarder (pip install nton; prod.keys required)"
	@echo "  make clean     -> remove build/, dist/, and legacy root artifacts"
else

.DEFAULT_GOAL := all

include $(DEVKITPRO)/libnx/switch_rules

TARGET      := NXSand
BUILD       := build
SOURCES     := source/game source/gpu source/sim source/ui source/platform source/platform/audio source/platform/input source/save
INCLUDES    := source third_party
ROMFS       := romfs

APP_TITLE   := NXSand
APP_AUTHOR  := antoi
APP_VERSION := 0.0.1
ICON        := romfs/icon.jpg

ARCH        := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS      := -g -Wall -O3 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS      += $(INCLUDE) -D__SWITCH__ -DNX_SWITCH=1
CXXFLAGS    := $(CFLAGS) -std=gnu++17
LDFLAGS     := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        := -lSDL2 -lfreetype -lharfbuzz -lpng -lz -lbz2 -lGLESv2 -lEGL -lglapi -ldrm_nouveau -lnx -lm

LIBDIRS     := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/build/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

export OFILES   := $(addsuffix .o,$(CFILES) $(CPPFILES))
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD) -I$(PORTLIBS)/include/SDL2 -I$(PORTLIBS)/include/freetype2

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export LD       := $(CXX)

# Paths must use TOPDIR: inner make runs from build/ (see devkitPro examples).
ROMFS_DIR          := $(TOPDIR)/$(ROMFS)
ROMFS_ICON         := $(ROMFS_DIR)/icon.jpg
ROMFS_SHADERS_STAMP := $(TOPDIR)/$(BUILD)/.stamp_shaders
export ROMFS_DIR ROMFS_ICON

ifeq ($(strip $(ICON)),)
	export APP_ICON := $(LIBNX)/default_icon.jpg
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

export NROFLAGS  += --icon=$(APP_ICON)
export NROFLAGS  += --nacp=$(OUTPUT).nacp
export NROFLAGS  += --romfsdir=$(ROMFS_DIR)

.PHONY: all clean prepare_romfs

prepare_romfs: $(ROMFS_SHADERS_STAMP) $(ROMFS_ICON)
	@rm -rf $(ROMFS_DIR)/fonts

$(ROMFS_SHADERS_STAMP): shaders/*.frag shaders/*.vert shaders/*.glsl shaders/*.comp $(TOPDIR)/Makefile
	@rm -rf $(ROMFS_DIR)/fonts
	@mkdir -p $(dir $@) $(ROMFS_DIR)/shaders
	@rm -f $(ROMFS_DIR)/shaders/*.frag $(ROMFS_DIR)/shaders/*.vert $(ROMFS_DIR)/shaders/*.glsl $(ROMFS_DIR)/shaders/*.comp
	@cp -f shaders/*.frag shaders/*.vert shaders/*.glsl shaders/*.comp $(ROMFS_DIR)/shaders/
	@touch $@

$(ROMFS_ICON):
	@echo "ERROR: $(ROMFS_ICON) missing."
	@echo "       Windows: powershell -File scripts/gen_icon.ps1"
	@echo "       Then run make again."
	@exit 1

all: prepare_romfs $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

else
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nro

# Repack NRO when romfs, hbmenu assets, or packaging flags change.
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp $(ROMFS_SHADERS_STAMP) $(ROMFS_ICON) $(APP_ICON) $(TOPDIR)/Makefile

$(OUTPUT).elf: $(OFILES)

%.cpp.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.c.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(INCLUDE) -c $< -o $@

-include $(DEPENDS)
endif

endif

# Stage Switch NRO for deploy/FTP (run after `make` in devkitPro shell).
dist:
	@mkdir -p dist/switch
	@test -f build/NXSand.nro || (echo "dist: missing build/NXSand.nro (run make first)" >&2; exit 1)
	@cp -f build/NXSand.nro dist/switch/NXSand.nro
	@echo "Staged: dist/switch/NXSand.nro"

# NSP forwarder via scripts/export-nsp.py (NTON + prod.keys; optional in CI).
nsp: dist
	@python3 scripts/export-nsp.py

clean:
	@rm -rf build dist
	@rm -f NXSand.nro NXSand.elf NXSand.nso NXSand NXSand.map NXSand.lst .map
	@rm -f NXEngine.nro NXEngine.elf NXEngine.nso NXEngine NXEngine.map NXEngine.lst .map
	@rm -f *.o *.cpp.o *.c.o NXSand.nacp NXEngine.nacp
