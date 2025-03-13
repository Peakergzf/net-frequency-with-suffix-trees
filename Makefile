CXXFLAGS   = -std=c++20 -Wall -Wextra -Wshadow -Wconversion
TARGET     = main
SRC_DIRS   = ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(addsuffix .o, $(basename $(SRCS)))

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIB)

.PHONY: clean run

clean:
	$(RM) $(TARGET) $(OBJS)

run:
	./$(TARGET)