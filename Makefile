override CXXFLAGS += -std=c++20
files = main.cpp
.PHONY: all
all: $(files:%.cpp=%.o)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o main $(files:%.cpp=%.o)

.PHONY: clean
clean: 
	@rm -f *.o main 
