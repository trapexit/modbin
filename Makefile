OUTPUT = modbin

STRIP  = strip
CFLAGS = -Os -I.

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OUTPUT)

$(OUTPUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(OBJ)
	$(STRIP) --strip-all $(OUTPUT)

static: $(OBJ)
	$(CC) -static $(CFLAGS) -o $(OUTPUT) $(OBJ)
	$(STRIP) --strip-all $(OUTPUT)

clean:
	rm -vf $(OUTPUT) $(OBJ)
