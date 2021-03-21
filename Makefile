FILES=main.c
TARGET=mole
FLAGS=-std=gnu99 -Wall -pedantic -g
L_FLAGS=-lpthread -lm
${TARGET}: ${FILES}
	gcc ${FLAGS} -o ${TARGET} ${FILES} ${L_FLAGS}
.PHONY: clean

clean:
	-rm -f ${TARGET}
