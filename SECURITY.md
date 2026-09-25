# Security Policy

## Belqen

Thanks for helping me keep **Belqen**, **Belqen-Kernel**, and **Belqen-OS** secure.

Belqen is an experimental operating system and kernel project that I am building for learning, experimentation, and low-level systems development. It is still in development and is **not production-ready**, so security issues may exist.

## Reporting a Vulnerability

If you find a potential security vulnerability in Belqen, please report it privately before making it public.

### Preferred method

The preferred way to report a security vulnerability is through **GitHub's Private Vulnerability Reporting** feature.

This allows you to privately report potential vulnerabilities directly to the repository maintainers without publicly exposing the issue.

Please use the **"Report a vulnerability"** option in the repository's **Security** tab.

### Alternative method

If you cannot use GitHub's private vulnerability reporting, you can contact me directly by email:

**[lexyfischer@web.de](mailto:lexyfischer@web.de)**

If you are not sure whether something is a security issue, please report it anyway.

## What to Report

Security issues can include things such as:

* Kernel privilege escalation
* User-mode to kernel-mode security issues
* Memory corruption
* Buffer overflows
* Out-of-bounds memory access
* Use-after-free vulnerabilities
* Memory-management issues with security implications
* Process or thread isolation problems
* System-call vulnerabilities
* Interrupt or exception-handling vulnerabilities
* Device-driver vulnerabilities
* PCI or USB security issues
* Storage-related vulnerabilities
* Networking vulnerabilities
* Packet or protocol parsing problems
* Boot or initialization vulnerabilities
* Information disclosure
* Denial-of-service issues affecting system security
* Bugs that allow unintended code execution or privilege escalation

## What to Include

When reporting a vulnerability, please include as much information as possible, such as:

* What the vulnerability is
* Which component is affected
* The affected version or commit, if known
* Steps to reproduce it
* A minimal proof of concept, if appropriate
* What you expected to happen
* What actually happened
* The possible security impact
* Relevant logs or debugging information
* Any workarounds you know about

Please do not include unnecessary personal or sensitive information.

## Responsible Disclosure

Please give me reasonable time to investigate and address a security vulnerability before publicly disclosing it.

Once I have investigated the issue and, where appropriate, fixed it, I may publicly document the vulnerability and its resolution.

The timing of a public disclosure may depend on the severity of the issue, whether a fix is available, and how difficult the fix is to develop and test.

## Security Updates

Security fixes may be released through the normal Belqen development and release channels.

Since Belqen is still actively being developed, fixing a security issue may require changes to the kernel, drivers, system interfaces, or other parts of the project.

I recommend keeping Belqen up to date when security fixes are released.

## Supported Versions

Belqen is currently under active development and does not currently have a long-term-support release policy.

| Version                     | Security Support          |
| --------------------------- | ------------------------- |
| Current development version | Supported where practical |
| Older development versions  | No guaranteed support     |
| Modified forks              | Not officially supported  |

Forks and derivative works are responsible for handling security issues in their own versions while still following the terms of the **Belqen Custom Non-Commercial License (BCNCL)**.

## Security Research

Security research, vulnerability discovery, debugging, and experimentation are welcome as part of Belqen's educational and experimental purpose.

However, please only test systems, hardware, networks, or services that you own or have explicit permission to test.

When testing Belqen, I recommend using isolated environments such as:

* Virtual machines
* Emulators
* Dedicated test hardware
* Isolated networks
* Disposable development environments

Belqen contains low-level code that can interact directly with hardware, memory, storage, networking, and other system resources, so testing should be done in an environment where unexpected failures can be safely contained.

## AI and Security Reports

Belqen is subject to the **Artificial Intelligence Prohibition** contained in the BCNCL.

The license prohibits providing Belqen source code or qualifying derivative-work source code to artificial intelligence systems for purposes such as analysis, debugging, explanation, modification, code review, or other AI processing.

Security researchers and contributors must make sure that their handling of Belqen source code follows the BCNCL.

This security policy does not modify or override the BCNCL.

## Scope

This security policy covers Belqen, including:

* Belqen-Kernel
* Belqen-OS
* Kernel components
* Device drivers
* System calls
* Process and thread management
* Memory management
* Interrupt and exception handling
* Networking
* Storage and device subsystems
* Build and boot components
* Other source code included in the project

Third-party software, hardware, dependencies, and external services are not directly maintained by me unless explicitly stated otherwise.

## License

Belqen is distributed under the **Belqen Custom Non-Commercial License (BCNCL) v1.0**.

Security research, vulnerability reports, contributions, and derivative works must comply with the applicable license terms.

See [`LICENSE`](LICENSE) for the complete license.

## Acknowledgements

I may publicly acknowledge security researchers who responsibly report vulnerabilities.

If you would prefer to remain anonymous, please let me know when reporting the vulnerability.

---

Copyright © 2026 lexyleinchen.

All rights reserved except for the rights expressly granted by the BCNCL.
