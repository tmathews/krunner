FLAGS = $(shell pkg-config --cflags --libs librsvg-2.0)

build: protocol
	cc -g -o krunner main.c waywrap/*.c\
		-lwayland-client -lwayland-cursor -lrt -lxkbcommon -lcairo -lm $(FLAGS)
		#$(pkg-config --cflags --libs cairo librsvg-2.0)

protocol:
	wayland-scanner private-code \
		< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
		> waywrap/xdg-shell-protocol.c
	wayland-scanner client-header \
		< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
		> waywrap/xdg-shell-client-protocol.h
	wayland-scanner private-code \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		> waywrap/xdg-decoration-protocol.c
	wayland-scanner client-header \
		< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		> waywrap/xdg-decoration-client-protocol.h

clean:
	rm -f waywrap/*-protocol.h waywrap/*-protocol.c krunner 

install:
	mkdir -p /usr/local/share/kallos/data/
	cp cmd.svg /usr/local/share/kallos/data/cmd.svg
	cp krunner /usr/local/bin/krunner

.PHONY: client protocol clean install
