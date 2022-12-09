#!/bin/bash
#g++ -g3 -Wall main.cpp gl_frontEnd.cpp level_io.cpp PacMan.cpp Ghost.cpp RGB.cpp -lm -lGL -lglut -lpthread -o test
cd Code-handout
g++ -Wall -Wno-deprecated -std=c++17 *.cpp -framework OpenGL -framework glut -o Final
./Final