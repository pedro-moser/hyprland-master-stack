CXX ?= g++
CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b

# g++ defaults to -fgnu-unique, which makes the static `ver` inside
# __hyprland_api_get_client_hash() shared across all plugins in the
# Hyprland process. If a stale plugin had been loaded earlier this session,
# the new build inherits its hash → spurious "Version mismatch". Same fix
# the upstream hyprland-plugins Makefile uses.
ifeq ($(CXX),g++)
    EXTRA_FLAGS = -fno-gnu-unique
else
    EXTRA_FLAGS =
endif

PKG_CONFIG_DEPS = pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon

SRCS = src/main.cpp src/MasterStackAlgorithm.cpp
OUT = master-stack.so

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(EXTRA_FLAGS) $(SRCS) \
	  -o $(OUT) \
	  `pkg-config --cflags $(PKG_CONFIG_DEPS)`

clean:
	rm -f ./$(OUT)

.PHONY: all clean
