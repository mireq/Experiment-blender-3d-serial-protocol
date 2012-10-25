.PHONY: all

all: render
	./render

render: render.cpp
	g++ $< -o $@ -lglut -lGL -lGLU -lm -lX11 -lXmu

