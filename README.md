# BTAP: A Security-Instrumented Compiler Backend

**BTAP (Backend Translation and Protection)** is an experimental compiler backend designed to explore the intersection of **High-Performance GPGPU Computing** and **Systems Security**. While most compilers prioritize throughput, BTAP investigates how security primitives—such as **Control-Flow Integrity (CFI)** and **Memory Safety Guards**—can be baked directly into the Intermediate Representation (IR).

This project targets a virtual Instruction Set Architecture (ISA), specifically modeling the challenges of register pressure and latency hiding in massively parallel environments.

## Core Features

- **Intermediate Representation (IR):** Utilizes a Three-Address Code (TAC) structure to facilitate robust optimization passes and data-flow analysis.
- **Security Instrumentation:** Implements a "Security Pass" that automatically injects memory-integrity guards and bounds-checking logic at the IR level, reducing the software attack surface.
- **PTX Lowering:** Models the translation from high-level abstractions to hardware-proximate Virtual ISA instructions (NVIDIA PTX).

## Architecture

1.  **Frontend:** C++-based lexer and parser stub for a subset of C-like syntax.
2.  **Optimizer:** Framework for Constant Folding, Dead Code Elimination, and basic register pressure estimation.
3.  **BTAP-Guard Pass:** The core security module that instruments code at the IR level to protect against buffer overflows and memory corruption.
4.  **Code Generation:** A modular backend emitting PTX-styled assembly optimized for a virtual GPU target.

## Why BTAP?

The goal is to demonstrate that "Secure by Design" principles can be implemented at the compiler level without compromising the **SIMT (Single Instruction, Multiple Threads)** execution model. By automating protection during the lowering phase, we ensure that security is a first-class citizen in the GPGPU toolchain.
