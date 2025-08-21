// by Markus Gulla

// includes
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>

// defines
#define CTRL_KEY(k) ((k) & 0x1f)

// data
struct config{
    uint16_t screen_rows;
    uint16_t screen_columns;
    struct termios orig_termios;
};

struct config e;

// terminal
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void rawToggleDisable()
{
   if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &e.orig_termios) == -1) {die("tcsetattr");}
}

void rawToggleEnable()
{
    if(tcgetattr(STDIN_FILENO, &e.orig_termios) == -1) {die("tcgetattr");}
    atexit(rawToggleDisable);

    struct termios raw = e.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
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
    return c;
}

int getWinSize(int *rows, int *columns)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// output
void drawRows()
{
    for(uint8_t i = 0; i < e.screen_columns; i++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void refreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    drawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

// input
void processKeypress()
{
    char c = editorReadKey();
    
    printf("%d (%c)",c,c);
    switch(c)
    {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

// init
void initEditor()
{
    if(getWinSize(&e.screen_rows, &e.screen_rows) == -1){die("getWinSize");}
}

int main()
{
    rawToggleEnable();
    initEditor();

    while (1)
    {
        refreshScreen();
        processKeypress();
    }
    return 0;
}