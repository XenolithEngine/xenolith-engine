# DSO FFI helper

This simple application used to enable dlopen for static musl-based application

See concept here: https://github.com/pfalcon/foreign-dlopen/tree/master

## Precompiled

For usage within toolchain, it's precompiled on oldest available linux version:

* x86_64: Ubuntu 16.04, kernel 4.15, GLIBC 2.23

## Usage

Precompiled files should be embedded within aplication with stappler_abi module.
In most cases, it will be set up automatically when you use stappler::Dso utility.
