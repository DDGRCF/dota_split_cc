# Dota Img Split for MMRotate[CC]

## Description 
This repo is for [MMRotate-1.x](https://github.com/open-mmlab/mmrotate.git). And You
can download MMRotate by 
```bash
git clone https://github.com/open-mmlab/mmrotate.git -b 1.x
```

And has tested on the gdal-3.0.2/3.0.4 proj-6.3.1 sqlite-3.4.10

## Why
In MMRotate, it has the img split program that is written in python, and it uses the cv2 to read image. It always load all images and split it. But it is impossible for images that is too large(like 5GB, 16GB 32GB per image...), which is normal in some remote sense.

Gdal is a C++ library for dealing remote sense. It can read images by a patch and patch. So, it is more reasonable for remote sense dealing.Gdal has support many program language, like c/c++, python, goland... And you can find it easily in github.

In many cases, some computers are not allowed to be connected to the Internet, so it may be inconvenient to download gdal and then compile it. Therefore, I use c/c++ to write, and minimize its dependence on external libraries. In the release, I released a cropping program that can be executed on ubutun16.04 and above. This program does not need to install any dependencies.

## How
build dependencies, two methods:
1. download the gdal proj sqlite from the office source. And build from the source
2. `sudo apt-get install -y gdal-bin && apt-get install -y libgdal-dev`

```bash
git clone https://github.com/DDGRCF/dota_split_cc.git
cd dota_split_cc
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```
