# BLModule

This is a BootLoader module to test the ARINC615A protocol. Use the Communication Manager to send and receive messages.

This repository is part of project [ARIEL](https://github.com/TCC-PES-2022).

To clone this repository, run:

    git clone https://github.com/TCC-PES-2022/BLModule.git
    cd BLModule
    git submodule update --init --recursive

Before building your project, you may need to install some dependencies. To do so, run:

    sudo apt install -y build-essential libcjson-dev libgcrypt-dev openssl libtinyxml2-dev

Export the instalation path to the environment:

    export DESTDIR=<path_to_install>

You can also define this variable in your `.bashrc` file. or when calling any make rule. The default installation path is `/tmp`.

To build, run:

    make deps && make

To install, run:

    make install

To execute, run:

    make run