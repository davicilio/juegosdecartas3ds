# Compila y ejecuta los tests de la lógica con el compilador del sistema.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
SRC     := ../source
OBJS    := test_main.c $(SRC)/core/cards.c $(SRC)/core/rng.c \
           $(SRC)/core/avatar.c $(SRC)/core/profile.c $(SRC)/core/miidata.c $(SRC)/core/cardback.c \
           $(SRC)/games/sieteymedia.c $(SRC)/games/cinquillo.c \
           $(SRC)/games/brisca.c $(SRC)/games/escoba.c \
           $(SRC)/games/chinchon.c $(SRC)/games/mus.c \
           $(SRC)/games/guinote.c $(SRC)/games/solitario.c $(SRC)/games/piramide.c $(SRC)/games/uno.c $(SRC)/games/rummikub.c \
           $(SRC)/games/as23.c $(SRC)/games/videopoker.c $(SRC)/games/mahjong.c $(SRC)/games/toma6.c $(SRC)/games/freecell.c $(SRC)/games/parchis.c $(SRC)/games/negocio.c $(SRC)/games/riichi.c $(SRC)/games/tresenraya.c $(SRC)/games/flota.c $(SRC)/games/bingo.c $(SRC)/games/tresillo.c $(SRC)/games/mierda.c $(SRC)/games/burro.c $(SRC)/games/reversi.c $(SRC)/games/oca.c $(SRC)/games/butifarra.c $(SRC)/games/pumba.c $(SRC)/games/julepe.c $(SRC)/games/truco.c $(SRC)/games/mona.c $(SRC)/games/cucu.c $(SRC)/games/treintayuna.c \
           $(SRC)/games/mentiroso.c $(SRC)/games/culo.c $(SRC)/games/poker.c \
           $(SRC)/games/blackjack.c $(SRC)/games/corazones.c $(SRC)/games/pocha.c \
           $(SRC)/games/conecta4.c $(SRC)/games/damas.c $(SRC)/games/domino.c

.PHONY: all run clean

all: run

run_tests: $(OBJS) $(wildcard $(SRC)/core/*.h $(SRC)/games/*.h)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $(OBJS)

run: run_tests
	./run_tests

clean:
	rm -f run_tests
