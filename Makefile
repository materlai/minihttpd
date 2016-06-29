

CC  :=gcc 
CXX :=g++

RM := rm -rf 

CFLAGS   := -fPIC  
LDFLAGS  :=


TRAGER := minihttpd
BUILD_SRC := build_src


.PHONY:all
all:$(TARGET)

$(TARGET):$(BUILD_SRC)
	cp src/$(TARGET)  .

.PHONY:$(BUILD_SRC)
$(BUILD_SRC):
	make -C src

.PHONY:clean
clean:
	$(RM) $(TARGET)
	make clean -C src 

