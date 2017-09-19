# vt100

| Project   | Hacked together terminal emulator |
| --------- | --------------------------------- |
| Author    | Richard James Howe                |
| Copyright | 2017 Richard James Howe           |
| License   | MIT                               |
| Email     | howe.r.j.89@gmail.com             |

This is a hacked together terminal emulator, derived from a simulator for a
hardware [VT100][] implementation written in [VHDL][] and tested on an [FPGA][] 
that I wrote as part of a [Forth CPU][] project. The project is a simple
[Terminal Emulator][], nothing special.

This project is meant to be:

* Small
* A Toy

This project is not meant to be:

* Useful
* A complete implementation of a [VT100][] or implement all [ANSI Escape Sequences][]

It requires [GLUT][], [OpenGL][], and a [C99][] compiler. Type 'make' to build an
executable called 'vt100'.

[GLUT]: https://en.wikipedia.org/wiki/FreeGLUT
[C99]: https://gcc.gnu.org/
[OpenGL]: https://www.opengl.org/
[Forth CPU]: https://github.com/howerj/forth-cpu
[VT100]: https://en.wikipedia.org/wiki/VT100
[VHDL]: https://en.wikipedia.org/wiki/VHDL
[FPGA]: https://en.wikipedia.org/wiki/Field-programmable_gate_array
[Terminal Emulator]: https://en.wikipedia.org/wiki/Terminal_emulator
[ANSI Escape Sequences]: https://en.wikipedia.org/wiki/ANSI_escape_code
