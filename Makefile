.PHONY: all

all: render
	cat /home/mirec/serial|./render

render: render.cpp
	g++ $< -o $@ -lglut -lGL -lGLU -lm -lX11 -lXmu

