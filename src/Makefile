

CC  := gcc 
CXX := g++

RM :=  rm -rf 

CFLAGS := -fPIC -g
LDFLAGS := 

SRC := $(wildcard *.cpp)
OBJ := $(SRC:.cpp=.o)



TARGET := minihttpd 


.PHONY:all
all:$(TARGET)
	

$(TARGET):$(OBJ)
	$(CXX)   $^  -o $(TARGET)   $(LDFLAGS)

$(OBJ): %.o:%.cpp
	$(CXX) $(CFLAGS) -c $<  -o $@    


.PHONY:clean
clean:
	$(RM)  $(OBJ)
	$(RM)  $(TARGET)
