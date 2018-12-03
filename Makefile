#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/devkitA64/base_rules

all: saltysd_spawner/saltysd_spawner.kip saltysd_plugin_example/saltysd_plugin_example.elf

saltysd_spawner/saltysd_spawner.kip: saltysd_spawner/data/saltysd_proc.elf
	@cd saltysd_spawner && make

saltysd_spawner/data/saltysd_proc.elf: saltysd_proc/saltysd_proc.elf
	@cp $< $@

saltysd_proc/saltysd_proc.elf: saltysd_proc/data/saltysd_bootstrap.elf saltysd_proc/data/saltysd_core.elf
	@cd saltysd_proc && make

saltysd_proc/data/saltysd_bootstrap.elf: saltysd_bootstrap/saltysd_bootstrap.elf
	@cp $< $@

saltysd_proc/data/saltysd_core.elf: saltysd_core/saltysd_core.elf
	@cp $< $@

saltysd_bootstrap/saltysd_bootstrap.elf:
	@cd saltysd_bootstrap && make

saltysd_core/saltysd_core.elf:
	@cd saltysd_core && make

saltysd_plugin_example/saltysd_plugin_example.elf:
	@cd saltysd_plugin_example && make

clean:
	@cd saltysd_core && make clean
	@cd saltysd_bootstrap && make clean
	@cd saltysd_proc && make clean
	@cd saltysd_spawner && make clean
	@cd saltysd_plugin_example && make clean
