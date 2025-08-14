#!/bin/bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install python3-pip
pip3 install pandas
sudo apt-get install cmake
git clone --recursive https://github.com/jessdagostini/miniGiraffe.git
cd miniGiraffe/
git checkout arch
bash install-deps.sh
make miniGiraffe
source set-env.sh
./miniGiraffe
g++ src/lower-input.cpp -o lower-input
chmod +x lower-input