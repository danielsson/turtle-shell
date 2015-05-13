
all: turtle
	@echo ""

turtle: main.c
	gcc main.c -o turtle -ansi -g -Wall -pedantic

