CXX ?= g++
CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b

PKG_CONFIG_DEPS = pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon

SRCS = src/main.cpp src/MasterMonocleAlgorithm.cpp
OUT = master-monocle.so

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRCS) \
	  -o $(OUT) \
	  `pkg-config --cflags $(PKG_CONFIG_DEPS)`

clean:
	rm -f ./$(OUT)

.PHONY: all clean
