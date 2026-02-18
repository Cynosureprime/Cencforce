CC = cc
CFLAGS = -O3 -pthread -Wall -std=gnu99

all: encforce

encforce: encforce.o charconv.o yarn.o
	$(CC) $(CFLAGS) -o encforce encforce.o charconv.o yarn.o

encforce.o: encforce.c enc_tables.h charconv.h sb_tables.h cjk_data.h yarn.h
	$(CC) $(CFLAGS) -c encforce.c

charconv.o: charconv.c charconv.h sb_tables.h cjk_data.h
	$(CC) $(CFLAGS) -c charconv.c

yarn.o: yarn.c yarn.h
	$(CC) $(CFLAGS) -c yarn.c

clean:
	rm -f *.o encforce

GITHUB_SSH = ssh -i /Users/dlr/.ssh/waffle2git -o IdentitiesOnly=yes
GITHUB_SRC = encforce.c charconv.c charconv.h enc_tables.h sb_tables.h \
             cjk_data.h gen_sb_tables.py gen_cjk_tables.py yarn.c yarn.h \
             Makefile README.md .gitignore

github:
	git add $(GITHUB_SRC)
	git commit -m "$(MSG)"
	GIT_SSH_COMMAND="$(GITHUB_SSH)" git push origin main
