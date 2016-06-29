

CC  :=gcc 
CXX :=g++

RM := rm -rf 

CFLAGS   := -fPIC  
LDFLAGS  :=

SRC :=$(wildcard src/*.cpp)
OBJ :=$(SRC:.cpp=.o)


TARGET := minihttpd

.PHONY:all
all:$(TARGET)


$(TARGET):$(OBJ)
	$(CXX)  $^ -o $@  $(LDFLAGS)

$(OBJ): %.o:%.cpp
	$(CXX) -c  $(CFLAGS)  $< -o $@

.PHONY:clean
clean:
	$(RM) $(OBJ)
	$(RM) $(TARGET)


