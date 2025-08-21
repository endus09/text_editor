// by Markus Gulla

// includes
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

// defines
#define CTRL_KEY(k) ((k) & 0x1f)

// data
struct termios orig_termios;

// terminal
void die(const char *s)
{
    perror(s);
    exit(1);
}

void rawToggleDisable()
{
   if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {die("tcsetattr");}
}

void rawToggleEnable()
{
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) {die("tcgetattr");}
    atexit(rawToggleDisable);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {die("tcsetattr");};
}

char editorReadKey()
{
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN) {die("read");}
    }
}

// output
void refreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

// input
void processKeypress()
{
    char c = editorReadKey();
    
    printf("%d (%c)",c,c);
    switch(c)
    {
        case CTRL_KEY('q'):
        exit(0);
        break;
    }
}

// init
int main()
{
    rawToggleEnable();

    while (1)
    {
        refreshScreen();
        processKeypress();
    }
    return 0;
}