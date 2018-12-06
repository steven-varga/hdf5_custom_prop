# Copyright (c) 2018 vargaconsulting, Toronto,ON Canada
# Author: Varga, Steven <steven@vargaconsulting.ca>


all: prop 
CXXFLAGS =  -g -O3 -std=c++17  -I./h5cpp
LIBS =  -lprofiler -lhdf5 -lz -ldl -lm

%.o : $(SRC_DIR)/%.cpp 
	$(CXX)   -$(INCLUDES) -o $@  $(CPPFLAGS) $(CXXFLAGS) -c $^

prop: prop.o
	$(CXX) $^ $(LIBS) -o $@

clean:
	@$(RM) *.o *.h5 prop

prop-leak: prop
	./prop
	valgrind ./prop

.PHONY: test

