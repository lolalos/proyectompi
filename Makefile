# Crear un nuevo Makefile correctamente configurado
cat > Makefile << 'EOL'
INCDIR = -I.
DBG    = -g
OPT    = -O3
CPP    = mpic++

# Agregar flags de OpenCV
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4)
OPENCV_LIBS = $(shell pkg-config --libs opencv4)

CFLAGS = $(DBG) $(OPT) $(INCDIR) $(OPENCV_CFLAGS)
LINK   = -lm $(OPENCV_LIBS)

# Archivos fuente y objetos
SRCS = segment.cpp
OBJS = $(SRCS:.cpp=.o)

# Regla principal: compila el ejecutable 'segment'
all: segment

# Compila los objetos
%.o: %.cpp
    $(CPP) $(CFLAGS) -c $< -o $@

# Enlaza el ejecutable
segment: $(OBJS)
    $(CPP) $(CFLAGS) -o $@ $(OBJS) $(LINK)

# Limpia los archivos objeto y el ejecutable
clean:
    /bin/rm -f segment *.o

# Limpia archivos temporales adicionales
clean-all: clean
    /bin/rm -f *~
EOL