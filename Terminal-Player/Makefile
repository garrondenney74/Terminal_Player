CC = gcc
CFLAGS = -Wall -Wextra -std=c99 `pkg-config --cflags raylib libavformat libavcodec libavutil`
LDFLAGS = `pkg-config --libs raylib libavformat libswresample libavcodec libavutil` -lX11 -lGL -lm -lpthread

SRC = src/main.c src/decoder.c
OBJ = $(SRC:.c=.o)
TARGET = music_player

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean