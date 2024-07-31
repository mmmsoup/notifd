CC = gcc
CFLAGS = -g -Wall `pkg-config --cflags gio-2.0`
LDFLAGS = -lpthread `pkg-config --libs gio-2.0`

BIN_NAME = notifd

$(BIN_NAME): main.o 
	$(CC) -o $(BIN_NAME) $(LDFLAGS) main.o 

main.o: main.c org_freedesktop_notifications.h
	$(CC) -c $(CFLAGS) main.c org_freedesktop_notifications.h

org_freedesktop_notifications.h: org.freedesktop.Notifications.xml
	echo -ne "#ifndef ORG_FREEDESKTOP_NOTIFICATIONS_H\n#define ORG_FREEDESKTOP_NOTIFICATIONS_H\n\n#include \"gio/gio.h\"\n\nconst gchar org_freedesktop_notifications_xml[] = " > org_freedesktop_notifications.h
	cat org.freedesktop.Notifications.xml | sed "s/\"/\\\\\"/g" | sed "s/^.*$$/\"\0\"/" >> org_freedesktop_notifications.h
	echo -e "\"\";\n\n#endif" >> org_freedesktop_notifications.h

clean:
	rm $(BIN_NAME)
	rm org_freedesktop_notifications.h
	rm *.gch
	rm *.o
