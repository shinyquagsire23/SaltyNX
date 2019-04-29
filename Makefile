#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/devkitA64/base_rules

all: sdcard_out/SaltySD/saltysd_core.elf sdcard_out/SaltySD/saltysd_proc.elf sdcard_out/atmosphere/saltysd_spawner.kip saltysd_plugin_example/saltysd_plugin_example.elf

saltysd_spawner/saltysd_spawner.kip:
	@cd saltysd_spawner && make

saltysd_proc/saltysd_proc.elf: saltysd_proc/data/saltysd_bootstrap.elf
	@cd saltysd_proc && make

saltysd_bootstrap/saltysd_bootstrap.elf:
	@cd saltysd_bootstrap && make

saltysd_core/saltysd_core.elf:
	@cd saltysd_core && make

saltysd_plugin_example/saltysd_plugin_example.elf:
	@cd saltysd_plugin_example && make

saltysd_proc/data/saltysd_bootstrap.elf: saltysd_bootstrap/saltysd_bootstrap.elf
	@mkdir -p saltysd_proc/data/
	@cp $< $@

sdcard_out/SaltySD/saltysd_core.elf: saltysd_core/saltysd_core.elf
	@mkdir -p sdcard_out/SaltySD/
	@cp $< $@

sdcard_out/SaltySD/saltysd_proc.elf: saltysd_proc/saltysd_proc.elf
	@mkdir -p sdcard_out/SaltySD/
	@cp $< $@

sdcard_out/atmosphere/saltysd_spawner.kip: saltysd_spawner/saltysd_spawner.kip
	@mkdir -p sdcard_out/atmosphere/
	@cp $< $@

clean:
	@rm -f saltysd_proc/data/*
	@rm -f saltysd_spawner/data/*
	@cd saltysd_core && make clean
	@cd saltysd_bootstrap && make clean
	@cd saltysd_proc && make clean
	@cd saltysd_spawner && make clean
	@cd saltysd_plugin_example && make clean
