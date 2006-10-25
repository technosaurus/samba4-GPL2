[LIBRARY::DYNCONFIG]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = dynconfig.o

PATH_FLAGS = -DCONFIGFILE=\"$(CONFIGFILE)\" \
	 -DBINDIR=\"$(BINDIR)\" -DLMHOSTSFILE=\"$(LMHOSTSFILE)\" \
	 -DLOCKDIR=\"$(LOCKDIR)\" -DPIDDIR=\"$(PIDDIR)\" -DDATADIR=\"$(DATADIR)\" \
	 -DLOGFILEBASE=\"$(LOGFILEBASE)\" -DSHLIBEXT=\"$(SHLIBEXT)\" \
	 -DCONFIGDIR=\"$(CONFIGDIR)\" -DNCALRPCDIR=\"$(NCALRPCDIR)\" \
	 -DSWATDIR=\"$(SWATDIR)\" -DSERVICESDIR=\"$(SERVICESDIR)\" \
	 -DPRIVATE_DIR=\"$(PRIVATEDIR)\" \
	 -DMODULESDIR=\"$(MODULESDIR)\" -DJSDIR=\"$(JSDIR)\" \
	 -DTORTUREDIR=\"$(TORTUREDIR)\" \
	 -DSETUPDIR=\"$(SETUPDIR)\" -DWINBINDD_SOCKET_DIR=\"$(WINBINDD_SOCKET_DIR)\"

dynconfig.o: dynconfig.c Makefile
	@echo Compiling $<
	@$(CC) `$(PERL) $(srcdir)/script/cflags.pl $@` $(CFLAGS) $(PICFLAG) $(PATH_FLAGS) -c $< -o $@
