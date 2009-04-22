# This file is part of mktorrent
# Copyright (C) 2007, 2009 Emil Renner Berthing
#
# mktorrent is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# mktorrent is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

.PHONY: strip indent clean install uninstall

$(program): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(program) $(LDFLAGS) $(LIBS)

allinone: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) -DVERSION="\"$(version)\"" -DALLINONE main.c -o $(program) $(LDFLAGS) $(LIBS)

strip:
	strip $(program)

indent:
	indent -kr -i8 *.c *.h

clean:
	rm -f $(program) *.o *.c~ *.h~

install: $(program)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m755 $(program) $(DESTDIR)$(PREFIX)/bin/$(program)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(program)
