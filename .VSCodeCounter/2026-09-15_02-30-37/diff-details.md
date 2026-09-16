# Diff Details

Date : 2026-09-15 02:30:37

Directory /home/lexyleinchen/osdev/PrintOS

Total : 62 files,  3301 codes, 0 comments, 751 blanks, all 4052 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [Makefile](/Makefile) | Makefile | 48 | 0 | 15 | 63 |
| [README.md](/README.md) | Markdown | 11 | 0 | 0 | 11 |
| [src/apps/calculator/calculator.cpp](/src/apps/calculator/calculator.cpp) | C++ | 2 | 0 | -1 | 1 |
| [src/apps/calculator/calculator.h](/src/apps/calculator/calculator.h) | C++ | -2 | 0 | 0 | -2 |
| [src/apps/diskmanager/diskmanager.h](/src/apps/diskmanager/diskmanager.h) | C++ | -2 | 0 | 0 | -2 |
| [src/apps/filebrowser/filebrowser.cpp](/src/apps/filebrowser/filebrowser.cpp) | C++ | -1 | 0 | 0 | -1 |
| [src/apps/filebrowser/filebrowser.h](/src/apps/filebrowser/filebrowser.h) | C++ | -2 | 0 | 0 | -2 |
| [src/apps/logs/logs.cpp](/src/apps/logs/logs.cpp) | C++ | 109 | 0 | 35 | 144 |
| [src/apps/logs/logs.h](/src/apps/logs/logs.h) | C++ | 4 | 0 | 2 | 6 |
| [src/apps/terminal/terminal.cpp](/src/apps/terminal/terminal.cpp) | C++ | -94 | 0 | -32 | -126 |
| [src/apps/terminal/terminal.h](/src/apps/terminal/terminal.h) | C++ | -3 | 0 | 0 | -3 |
| [src/apps/texteditor/texteditor.c](/src/apps/texteditor/texteditor.c) | C | 2 | 0 | -1 | 1 |
| [src/apps/texteditor/texteditor.h](/src/apps/texteditor/texteditor.h) | C++ | -2 | 0 | 0 | -2 |
| [src/boot/boot.asm](/src/boot/boot.asm) | x86 and x86_64 Assembly | 2 | 0 | 1 | 3 |
| [src/kernel/boot/multiboot.c](/src/kernel/boot/multiboot.c) | C | 22 | 0 | 7 | 29 |
| [src/kernel/boot/multiboot.h](/src/kernel/boot/multiboot.h) | C++ | 1 | 0 | 1 | 2 |
| [src/kernel/core/log.c](/src/kernel/core/log.c) | C | 53 | 0 | 14 | 67 |
| [src/kernel/core/log.h](/src/kernel/core/log.h) | C++ | 3 | 0 | 3 | 6 |
| [src/kernel/core/work.c](/src/kernel/core/work.c) | C | 21 | 0 | 6 | 27 |
| [src/kernel/core/work.h](/src/kernel/core/work.h) | C++ | 2 | 0 | 1 | 3 |
| [src/kernel/drivers/ethernet/e1000/e1000.c](/src/kernel/drivers/ethernet/e1000/e1000.c) | C | 306 | 0 | 62 | 368 |
| [src/kernel/drivers/ethernet/e1000/e1000.h](/src/kernel/drivers/ethernet/e1000/e1000.h) | C++ | 15 | 0 | 9 | 24 |
| [src/kernel/drivers/ethernet/ethernet.c](/src/kernel/drivers/ethernet/ethernet.c) | C | 80 | 0 | 17 | 97 |
| [src/kernel/drivers/ethernet/ethernet.h](/src/kernel/drivers/ethernet/ethernet.h) | C++ | 25 | 0 | 10 | 35 |
| [src/kernel/drivers/ide/ide.c](/src/kernel/drivers/ide/ide.c) | C | 0 | 0 | -2 | -2 |
| [src/kernel/drivers/ide/ide.h](/src/kernel/drivers/ide/ide.h) | C++ | 2 | 0 | 2 | 4 |
| [src/kernel/drivers/pci/pci.c](/src/kernel/drivers/pci/pci.c) | C | 12 | 0 | 2 | 14 |
| [src/kernel/drivers/pci/pci.h](/src/kernel/drivers/pci/pci.h) | C++ | 3 | 0 | 3 | 6 |
| [src/kernel/drivers/ps2/ps2.c](/src/kernel/drivers/ps2/ps2.c) | C | 39 | 0 | 12 | 51 |
| [src/kernel/drivers/ps2/ps2.h](/src/kernel/drivers/ps2/ps2.h) | C++ | 3 | 0 | 3 | 6 |
| [src/kernel/drivers/serial/serial.c](/src/kernel/drivers/serial/serial.c) | C | 34 | 0 | 7 | 41 |
| [src/kernel/drivers/serial/serial.h](/src/kernel/drivers/serial/serial.h) | C++ | 13 | 0 | 7 | 20 |
| [src/kernel/framebuffer/framebuffer\_console.c](/src/kernel/framebuffer/framebuffer_console.c) | C | 177 | 0 | 21 | 198 |
| [src/kernel/framebuffer/framebuffer\_console.h](/src/kernel/framebuffer/framebuffer_console.h) | C++ | 12 | 0 | 5 | 17 |
| [src/kernel/inputs/keyboard.c](/src/kernel/inputs/keyboard.c) | C | 100 | 0 | 27 | 127 |
| [src/kernel/inputs/keyboard.h](/src/kernel/inputs/keyboard.h) | C++ | 1 | 0 | 1 | 2 |
| [src/kernel/inputs/mouse.c](/src/kernel/inputs/mouse.c) | C | -7 | 0 | -3 | -10 |
| [src/kernel/interrupts/apic.c](/src/kernel/interrupts/apic.c) | C | 390 | 0 | 92 | 482 |
| [src/kernel/interrupts/apic.h](/src/kernel/interrupts/apic.h) | C++ | 19 | 0 | 13 | 32 |
| [src/kernel/interrupts/interrupts.asm](/src/kernel/interrupts/interrupts.asm) | x86 and x86_64 Assembly | 128 | 0 | 16 | 144 |
| [src/kernel/interrupts/interrupts.c](/src/kernel/interrupts/interrupts.c) | C | 616 | 0 | 94 | 710 |
| [src/kernel/interrupts/interrupts.h](/src/kernel/interrupts/interrupts.h) | C++ | 45 | 0 | 16 | 61 |
| [src/kernel/kernel.c](/src/kernel/kernel.c) | C | 30 | 0 | 0 | 30 |
| [src/kernel/network/arp/arp.c](/src/kernel/network/arp/arp.c) | C | 134 | 0 | 28 | 162 |
| [src/kernel/network/arp/arp.h](/src/kernel/network/arp/arp.h) | C++ | 13 | 0 | 7 | 20 |
| [src/kernel/network/dhcp/dhcp.c](/src/kernel/network/dhcp/dhcp.c) | C | 268 | 0 | 57 | 325 |
| [src/kernel/network/dhcp/dhcp.h](/src/kernel/network/dhcp/dhcp.h) | C++ | 13 | 0 | 7 | 20 |
| [src/kernel/network/icmp/icmp.c](/src/kernel/network/icmp/icmp.c) | C | 76 | 0 | 21 | 97 |
| [src/kernel/network/icmp/icmp.h](/src/kernel/network/icmp/icmp.h) | C++ | 12 | 0 | 6 | 18 |
| [src/kernel/network/ipv4/ipv4.c](/src/kernel/network/ipv4/ipv4.c) | C | 164 | 0 | 40 | 204 |
| [src/kernel/network/ipv4/ipv4.h](/src/kernel/network/ipv4/ipv4.h) | C++ | 22 | 0 | 14 | 36 |
| [src/kernel/network/ipv6/ipv6.c](/src/kernel/network/ipv6/ipv6.c) | C | 27 | 0 | 6 | 33 |
| [src/kernel/network/ipv6/ipv6.h](/src/kernel/network/ipv6/ipv6.h) | C++ | 15 | 0 | 7 | 22 |
| [src/kernel/network/udp/udp.c](/src/kernel/network/udp/udp.c) | C | 49 | 0 | 11 | 60 |
| [src/kernel/network/udp/udp.h](/src/kernel/network/udp/udp.h) | C++ | 13 | 0 | 7 | 20 |
| [src/os/app.h](/src/os/app.h) | C++ | 1 | 0 | 1 | 2 |
| [src/os/desktop.cpp](/src/os/desktop.cpp) | C++ | 102 | 0 | 23 | 125 |
| [src/os/desktop.h](/src/os/desktop.h) | C++ | 6 | 0 | 4 | 10 |
| [src/os/graphics.cpp](/src/os/graphics.cpp) | C++ | 19 | 0 | 5 | 24 |
| [src/os/taskbar.cpp](/src/os/taskbar.cpp) | C++ | 36 | 0 | 8 | 44 |
| [src/os/ui.cpp](/src/os/ui.cpp) | C++ | 109 | 0 | 31 | 140 |
| [src/os/ui.h](/src/os/ui.h) | C++ | 5 | 0 | 3 | 8 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details