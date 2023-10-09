SRC_DIR = .
INCLUDE_DIR = 

LIB_NAME = ../engine/libtransom_memcpy.so

SOURCES  = $(wildcard $(SRC_DIR)/transom_memcpy.cpp)
OBJECTS  = $(SOURCES:.cpp=.o)
# supported_arches = ['5.0', '5.2', '5.3', '6.0', '6.1', '6.2',
#                         '7.0', '7.2', '7.5', '8.0', '8.6', '8.7', '8.9', '9.0']
CC = nvcc
CFLAGS  = -O0 -Xcompiler -fPIC -gencode=arch=compute_60,code=sm_60 \
								-gencode=arch=compute_61,code=sm_61 \
								-gencode=arch=compute_62,code=sm_62 \
								-gencode=arch=compute_70,code=sm_70 \
								-gencode=arch=compute_75,code=sm_75 \
								-gencode=arch=compute_80,code=sm_80
LDFLAGS =  -shared -lrt -lcudart -lcuda 
# CC = g++
# CFLAGS  = -I $(INCLUDE_DIR)  -std=c++2a -Ofast   -fPIC  -march=native
# LDFLAGS =  -shared -lrt

.PHONY: all clean

all: $(LIB_NAME)
clean:
	rm -f $(OBJECTS) $(LIB_NAME)

$(LIB_NAME): $(OBJECTS) 
	$(CC)  $(OBJECTS) $(LDFLAGS) -o $@  

$(OBJECTS): $(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

