mkdir -p bin
g++ -std=c++20 -I./include -L/usr/lib/x86_64-linux-gnu \
$(pkg-config --cflags gio-2.0) resources/resources.c main.cpp lib/AppBootstrapper.cpp \
lib/PythonManager.cpp lib/SpecConfig.cpp lib/Utils.cpp -g -o main -O3 -Wall \
$(pkg-config --libs gio-2.0) -lINIReader