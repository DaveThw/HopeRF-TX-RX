# Objects to link together - Make knows how to make .o from .c
OBJ=app_main.o decoder.o dev_HRF.o

# Libraries to link in to the final executable
LIBS=bcm2835

# Name of our application
APP_NAME=hoperf

CFLAGS=-Wall

$(APP_NAME): $(OBJ)
	gcc $(CFLAGS) -o $@ $^ -l$(LIBS)

clean:
	rm $(OBJ) $(APP_NAME)
