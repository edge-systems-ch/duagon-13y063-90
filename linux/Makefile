TOOL     = 13Y063
API      = standard
BIN_DIR  = bin

ifeq ($(API),minimal)
LIBPATH = $(realpath ./hidapi_minimal)
INCS += -I$(realpath ./hidapi_minimal)
OBJS = $(LIBPATH)/hidapi_minimal_13y063-90.o 13Y063-90_minimal.o
EXEC = $(BIN_DIR)/$(TOOL)-90_minimal
else
INCS = -I../hidapi/hidapi `pkg-config libusb-1.0 --cflags`
LIBS = -lpthread `pkg-config libusb-1.0 --libs`
OBJS = ../$(TOOL).o ../hidapi/libusb/hid.o ../cJSON/cJSON.o
EXEC = $(BIN_DIR)/$(TOOL)-90
PATCH = 0001-add-geo-address.patch
endif

.PHONY: all $(PATCH) clean

all: $(PATCH) $(EXEC)

$(PATCH):
	@if [ ! -f .patch_applied ]; then \
		patch -d ../hidapi -p1 < $(PATCH) && touch .patch_applied; \
	else \
		echo "patch already applied - skip"; \
	fi

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCS) $< -o $@

$(BIN_DIR):
	@mkdir -p $@

clean:
	@if [ -f .patch_applied ]; then \
		echo "remove patch"; \
		patch -R -d ../hidapi -p1 < $(PATCH) && $(RM) -rv .patch_applied; \
	fi
	@$(RM) -rv $(BIN_DIR) $(OBJS)

