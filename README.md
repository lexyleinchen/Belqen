# Belqen

**A custom-built operating system and kernel project written from scratch.**

**Belqen** stands for **Base Environment for Learning, Questioning, Engineering, and Noticing**.

Belqen is a personal operating system project built for learning, experimentation, and low-level systems development. The project includes **Belqen-Kernel**, the core kernel, and **Belqen-OS**, the reference operating system built around it.

## About

Belqen is developed from the ground up with a focus on understanding how operating systems work at a low level.

The project currently includes work on areas such as:

* Custom kernel development
* Hardware and device communication
* PS/2 controller support
* PS/2 keyboard input and scancodes
* PS/2 mouse input and movement
* Framebuffer graphics
* Basic graphical OS components
* Desktop and taskbar
* Terminal functionality
* Application system and app registry
* App icons on desktop
* Window/UI system
* Memory and system management
* Low-level C/C++ development
* Custom build systems and tooling
* PCI device support
* IDE/storage support
* USB subsystem
* Work/task scheduling system
* Networking using arp/ipv4 and ipv6
* Interrupt and exception handling
* Processes and threads
* User-mode execution and system calls

Belqen is an ongoing project and is intended primarily for experimentation, learning, and development.

## What is Belqen?

**Belqen** stands for:

> **Base Environment for Learning, Questioning, Engineering, and Noticing**

* **B — Base**
* **E — Environment**
* **L — Learning**
* **Q — Questioning**
* **E — Engineering**
* **N — Noticing**

**Belqen-Kernel** is the core kernel of the project, providing the foundational functionality of the system.

**Kernel Architecture**

Belqen uses a **monolithic kernel architecture** rather than a microkernel architecture.

The core operating system functionality, including hardware communication, device drivers, memory management, interrupt handling, scheduling, networking, and other low-level system services, is implemented within the kernel or operates as part of the kernel's core environment.

This architecture is intentionally used as part of Belqen's goal of learning and experimenting with low-level operating system design and understanding how the different components of an operating system interact directly with the kernel.

**Belqen-OS** is the reference OS built around Belqen-Kernel, providing the complete user environment.

The name reflects the project's purpose: **building a foundation for learning, questioning how systems work, engineering new ideas, and noticing what happens beneath the surface.**

## Source Code

**The Belqen source code is 100% human-written.**

The source code was created entirely by the Author, **lexyleinchen**, without the use of:

* Generative AI
* AI coding assistants
* AI-generated source code
* AI-generated modifications
* AI-generated contributions

AI was used only to assist with drafting and wording of certain project documentation, including this README and the accompanying custom license. AI assistance was **not** used to write the Belqen source code.

## License

Belqen is distributed under the **Belqen Custom Non-Commercial License (BCNCL) v1.0**.

See the [`LICENSE`](LICENSE) file for the complete terms.

### Important Restrictions

Under the BCNCL:

* Credit must always be given to **lexyleinchen**.
* Commercial use is not permitted without prior written permission.
* Commercial permission must be provided in a physical written document personally signed by the Author.
* Selling, licensing, renting, leasing, monetizing, or commercially exploiting Belqen or qualifying derivative works is prohibited without that permission.
* Forks and qualifying derivative works remain subject to the PCNCL.
* The license and its restrictions must not be removed or bypassed.
* AI processing of the source code is prohibited.
* AI-generated source code may not be added to Belqen or qualifying derivative works.

Please read the full `LICENSE` file before using, modifying, forking, or distributing this project.

## Forks and Derivative Works

Forking Belqen does not remove the requirements of the BCNCL.

A fork, modification, port, extension, integration, or other work that qualifies as a derivative work under the license must continue to comply with the license, including its:

* Attribution requirements
* Non-commercial requirements
* AI restrictions
* License-preservation requirements
* Redistribution restrictions

Simply changing the project name, repository, programming language, build system, or structure does not automatically remove these requirements.

## Building from Source

To build Belqen yourself, you need a suitable cross-compilation toolchain.

The build environment requires:

* GCC cross-compiler
* G++ cross-compiler
* GNU `ld` from the cross-compilation toolchain
* NASM
* GNU Make

The cross-compiler should be configured for the target architecture used by Belqen rather than compiling the kernel with the host system's default compiler.

Once the required toolchain is installed and configured, clone the repository and run the following commands from the Belqen project directory:

```bash
make clean
make
make iso
```

### Build Commands

**1. Clean previous build files**

```bash
make clean
```

Removes previous build artifacts so the project can be rebuilt from a clean state.

**2. Build Belqen**

```bash
make
```

Compiles and links Belqen-Kernel and the Belqen-OS components.

**3. Create the ISO**

```bash
make iso
```

Creates a bootable Belqen-OS ISO from the compiled project.

The resulting ISO can then be used with an emulator, virtual machine, or compatible physical hardware.

> **Note:** The exact cross-compiler target and toolchain configuration depend on the architecture and build configuration used by the current Belqen source tree.

## Prebuilt ISO

You do **not** need to build Belqen yourself if you only want to try the operating system.

A prebuilt ISO is provided with the project's **GitHub Releases**.

Download the latest [`releases`](release) ISO and use it directly with a virtual machine, emulator, or compatible hardware.

This is the recommended option if you simply want to test Belqen-OS without setting up the complete development and cross-compilation environment.

## Development

Belqen is an experimental project and is actively developed over time.

The architecture, APIs, features, and implementation may change without notice.

If you are studying the project, you are encouraged to read the source code and understand how the individual components interact.

## Project Status

**Status:** In Development

Belqen is not intended to be considered a finished or production-ready operating system.

Features may be incomplete, unstable, experimental, or subject to significant changes.

## Author

**lexyleinchen**

Belqen is independently developed as a custom operating system and kernel project.

## AI Documentation Disclosure

Parts of the Belqen project documentation and repository support files were drafted with assistance from generative AI.

This includes:

* `README.md`
* `LICENSE`
* `SECURITY.md`
* `CODE_OF_CONDUCT.md`
* `CONTRIBUTING.md`
* GitHub issue templates
* GitHub pull request templates

Generative AI assistance was used **only for documentation, wording, structure, and related non-source-code text**.

All AI-assisted text was reviewed by the Author, **lexyleinchen**, and was edited, revised, or otherwise adjusted where necessary before being included in the project.

This disclosure applies **only to documentation, repository metadata, templates, and other non-source-code text**.

The Belqen source code itself remains **100% human-written** and was created by the Author, **lexyleinchen**, without the use of generative AI, AI coding assistants, or AI-generated source code.

No AI-generated source code or AI-generated source-code modifications have been added to Belqen.

## Copyright

Copyright © 2026 lexyleinchen.

All rights reserved except for the rights expressly granted by the BCNCL.

See [`LICENSE`](LICENSE) for the complete license terms.
