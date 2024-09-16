all: vs_port v_switch

vs_port: vs_port.c logger.c
	$(CC) -o vs_port vs_port.c logger.c -Wall -Wextra -Wpedantic -std=c11

v_switch: v_switch.c logger.c
	$(CC) -o v_switch v_switch.c logger.c `pkg-config --libs glib-2.0 --cflags glib-2.0` -lcli -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o vs_port v_switch

.PHONY: all clean
