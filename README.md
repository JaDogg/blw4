# 32-Blit WASM4

![](https://github.com/JaDogg/blw4/blob/main/wasm4-blit32.png?raw=true)

## What is this?

* WASM4 fantasy console emulator for 32Blit.
* Can load a game cart file named `cart.wasm`.
* Repo created using 32blit boilerplate

## Controls

| WASM4 Control | 32Blit Control  |
|---------------|-----------------|
| X             | X               |
| Z             | Y               |
| Up            | Up              |
| Down          | Down            |
| Left          | Left            |
| Right         | Right           |
| Mouse Left    | A               |
| Mouse Right   | B               |
| Mouse Middle  | Joystick Button |
| Mouse Move    | Joystick        |


## Possible Problems:

* Possibly != 60FPS (Do not think this is possible to fix)
* Possibly <64KB RAM for WASM4 (Need to check in 32blit)
* No net-play (Do not think this is possible to fix)
* No launcher (Cart file must be `cart.wasm`) (Can attempt to implement this)
* Sound working but not at 60FPS :(
    * Problem: should this be implemented as 40FPS or 60FPS?
* No disk save/load support. (Can attempt to implement this)
* Not part of original wasm4 github repo (At the moment I did not expect this to work at all, so I didn't fork it)
    * Various changes cross files were done.

## Usage

* Copy the `blw4.blit` then `cart.wasm` file to flash storage.
* You can find more carts at https://wasm4.org/play

----------

# Notes

## Setting up SDKs

### Installing cross-compiler and C library, sdl2

```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-binutils sdl2 sdl2_image sdl2_net
python3 -m pip install 32blit
```

### Cloning SDKs

```bash
# I couldn't get the auto download work. So I cloned it to parent directory of this folder
git clone --recurse-submodules -j8 git@github.com:raspberrypi/pico-sdk.git
git clone --recurse-submodules -j8 git@github.com:pimoroni/picosystem.git
git clone --recurse-submodules -j8 git@github.com:32blit/32blit-sdk
git clone --recurse-submodules -j8 git@github.com:raspberrypi/pico-extras
```

### Cloning original WASM4 (You do not need this)

```bash
git clone --recurse-submodules -j8 git@github.com:aduros/wasm4.git
```

---------

# License

```
Copyright (c) Bruno Garcia
Copyright (c) 2022 Bhathiya Perera

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
```

# Cart file

https://wasm4.org/play/nyancat
Jake Ledoux
https://creativecommons.org/licenses/by-nc-sa/4.0/

# Init file system function

```
MIT License

Copyright (c) 2020 Charlie Birks

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

# WASM3

```
MIT License

Copyright (c) 2019 Steven Massey, Volodymyr Shymanskyy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
