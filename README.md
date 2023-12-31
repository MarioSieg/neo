# NEO AI Programming Language (WIP)

NEO is a small but powerful, fast, and object-oriented scripting language that draws inspiration from the best aspects of Java, Lua, and Python.<br>
It is designed to be used for writing AI and ML systems, and it is easily embeddable into any application, bringing AI into various projects.

## Key Features

- **AI (Tensors) & Optimized Algorithms** 🧠: NEO comes equipped with built-in AI capabilities and optimized algorithms for CPU (SSE, AVX, AVX-512, Neon) and GPU (Cuda, OpenCL, Metal) using a JIT compiler.

- **AI Computation Graph** 📈: NEO's AI computation graph allows you to easily create and train neural networks.

- **Lightweight & Efficient** 🚀: NEO prioritizes efficiency for high-performance applications.

- **Object-Oriented** 🧩: Embrace modularity with object-oriented programming.

- **Easy Integration** 🧬: Seamlessly embed the NEO compiler and runtime via the C99 library for hassle-free extensibility.

- **Familiar Syntax** 🤓: If you know Java, Lua, or Python, NEO's syntax will feel like home.

- **Static Typing** 📝: Write safe and expressive code with NEO's static typing.

- **Automated Memory Management** 🗑️: NEO's robust garbage collector handles memory for you.

- **Comprehensive Standard Library** 📚: Access a rich set of tools and features out of the box.


## Platform Support

| Platform       | Architecture | Status  |
|----------------|--------------|---------|
| Linux          | x86-64       | ✅     |
| Linux          | AArch64      | 🚧     |
| MacOS          | x86-64       | 🚧     |
| MacOS          | AArch64      | 🚧     |
| Windows        | x86-64       | ✅     |
| Windows        | AArch64      | 🚧     |

**Legend:**
- ✅ Supported
- 🚧 In Progress

## Building

NEO uses the CMake build system.<br>
- <b>Build compiler and runtime</b>: A C99 compatible compiler (tested with GCC, Clang, and MSVC).<br>
- <b>Build unit tests and fuzzer</b>: A C++ 20 compatible compiler is required to run unit tests and the fuzzer

## IDE Support
Get the [Visual Studio Code extension (WIP)](https://github.com/MarioSieg/neo_vscode_extensions)
![Rectangle class](https://i.imgur.com/xTPbuT3.png)
