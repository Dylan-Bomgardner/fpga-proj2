arm-linux-gnueabihf-g++ main.cpp -o myprogram -static \
  -I/usr/arm-linux-gnueabihf/include \
  -L/usr/arm-linux-gnueabihf/lib \
  -lmpg123

#   arm-linux-gnueabi-g++ main.cpp -o myprogram -static \
#   -I/usr/arm-linux-gnueabi/include \
#   -L/usr/arm-linux-gnueabi/lib \
#   -lmpg123 \
#   -gdwarf-4

scp myprogram root@192.168.3.2:~