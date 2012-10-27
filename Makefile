.PHONY: all

all: render render-mips
	cat /home/mirec/serial|./render

render: render.cpp
	g++ -DRUN_ON_PC $< -o $@ -lglut -lGL -lGLU -lm -lX11 -lXmu

render-mips: render.cpp
	mips2_fp_le-c++ -DRUN_ON_TV -mips32 -static $< -o $@

