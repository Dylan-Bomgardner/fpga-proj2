# fpga-proj2


``` bash
wget https://www.mpg123.de/download/mpg123-1.32.5.tar.bz2
tar xf mpg123-1.32.5.tar.bz2
cd mpg123-1.32.5

./configure --host=arm-linux-gnueabihf \
  --prefix=/usr/arm-linux-gnueabihf \
  --enable-static --disable-shared \
  CC=arm-linux-gnueabihf-gcc \
  CXX=arm-linux-gnueabihf-g++

make
sudo make install
```