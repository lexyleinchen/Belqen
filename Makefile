TARGET = x86_64-elf

CC = $(TARGET)-gcc
CXX = $(TARGET)-g++
LD = $(TARGET)-ld
AS = nasm
APP_DIRS := $(wildcard src/apps/*)
APP_SOURCES := $(foreach dir,$(APP_DIRS),$(wildcard $(dir)/*.cpp))
APP_OBJECTS := $(patsubst src/%.cpp,build/%.o,$(APP_SOURCES))

CFLAGS = -ffreestanding \
	-fno-stack-protector \
	-fno-omit-frame-pointer \
	-fno-pie \
	-mno-red-zone \
	-Wall \
	-Wextra

CXXFLAGS = -ffreestanding \
	-fno-stack-protector \
	-fno-omit-frame-pointer \
	-fno-rtti \
	-fno-exceptions \
	-fno-pie \
	-mno-red-zone \
	-Wall \
	-Wextra

LDFLAGS = -T src/linker.ld

KERNEL = build/kernel.bin

all: $(KERNEL)

build:
	mkdir -p build

build/kernel.o: src/kernel/kernel.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -Isrc/os -c $< -o $@

build/interrupts.o: src/kernel/interrupts/interrupts.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/interrupts_asm.o: src/kernel/interrupts/interrupts.asm | build
	$(AS) -f elf64 $< -o $@

build/apic.o: src/kernel/interrupts/apic.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/pmm.o: src/kernel/memory/pmm.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/vmm.o: src/kernel/memory/vmm.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/heap.o: src/kernel/memory/heap.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/malloc.o: src/kernel/memory/malloc.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/address_space.o: src/kernel/memory/address_space.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/log.o: src/kernel/core/log.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/work.o: src/kernel/core/work.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/framebuffer.o: src/kernel/framebuffer/framebuffer.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/framebuffer_console.o: src/kernel/framebuffer/framebuffer_console.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/block.o: src/kernel/storage/block.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/storage.o: src/kernel/storage/storage.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/partition.o: src/kernel/storage/partition/partition.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/filesystem.o: src/kernel/storage/partition/filesystem/filesystem.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/fat32.o: src/kernel/storage/partition/filesystem/fat32/fat32.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/multiboot.o: src/kernel/boot/multiboot.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ps2.o: src/kernel/drivers/ps2/ps2.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/pci.o: src/kernel/drivers/pci/pci.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/usb.o: src/kernel/drivers/usb/usb.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ahci.o: src/kernel/drivers/ahci/ahci.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ethernet.o: src/kernel/drivers/ethernet/ethernet.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/e1000.o: src/kernel/drivers/ethernet/e1000/e1000.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/serial.o: src/kernel/drivers/serial/serial.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/arp.o: src/kernel/network/arp/arp.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ipv4.o: src/kernel/network/ipv4/ipv4.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ipv6.o: src/kernel/network/ipv6/ipv6.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/icmp.o: src/kernel/network/icmp/icmp.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/udp.o: src/kernel/network/udp/udp.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/dhcp.o: src/kernel/network/dhcp/dhcp.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/ide.o: src/kernel/drivers/ide/ide.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/keyboard.o: src/kernel/inputs/keyboard.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/mouse.o: src/kernel/inputs/mouse.c | build
	$(CC) $(CFLAGS) -Isrc/kernel -c $< -o $@

build/boot.o: src/boot/boot.asm | build
	$(AS) -f elf64 $< -o $@

build/os.o: src/os/os.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -Isrc/kernel -c $< -o $@

build/graphics.o: src/os/graphics.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/ui.o: src/os/ui.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/font.o: src/os/font.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/desktop.o: src/os/desktop.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/taskbar.o: src/os/taskbar.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/os_mouse.o: src/os/os_mouse.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc/os -c $< -o $@

build/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc/os -Isrc/kernel -Isrc/apps -c $< -o $@

$(KERNEL): build/boot.o \
		build/kernel.o \
		build/interrupts.o \
		build/interrupts_asm.o \
		build/apic.o \
		build/pmm.o \
		build/vmm.o \
		build/heap.o \
		build/malloc.o \
		build/address_space.o \
		build/log.o \
		build/work.o \
		build/framebuffer.o \
		build/framebuffer_console.o \
		build/block.o \
		build/storage.o \
		build/partition.o \
		build/filesystem.o \
		build/fat32.o \
		build/multiboot.o \
		build/ps2.o \
		build/pci.o \
		build/usb.o \
		build/ahci.o \
		build/ethernet.o \
		build/e1000.o \
		build/serial.o \
		build/arp.o \
		build/ipv4.o \
		build/ipv6.o \
		build/icmp.o \
		build/udp.o \
		build/dhcp.o \
		build/ide.o \
		build/keyboard.o \
		build/mouse.o \
		build/os.o \
		build/graphics.o \
		build/ui.o \
		build/font.o \
		build/desktop.o \
		build/taskbar.o \
		build/os_mouse.o \
		$(APP_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

iso: $(KERNEL)
	mkdir -p iso/boot/grub
	cp $(KERNEL) iso/boot/kernel.bin
	grub2-mkrescue -o build/PrintOS.iso iso

clean:
	rm -rf build
	rm -rf iso/boot/kernel.bin

.PHONY: all iso clean
