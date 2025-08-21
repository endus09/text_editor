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
#include <string.h>

// defines
#define CTRL_KEY(k) ((k) & 0x1f)

// data
struct config
{
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

int cursorPosition(uint16_t *rows, uint16_t *columns)
{
    char buf[32];
    uint8_t i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {return -1;}

    while( i < sizeof(buf) - 1)
    {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) {break;}
        if(buf[i] == 'R'){break;}
        i++;
    }
    buf[i] = '\0';

    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

    if(buf[0] != '\x1b' || buf[1] != '[') {return -1;}
    if(sscanf(&buf[2], "%d;%d", rows, columns) != 2) {return -1;}

    return 0;
}

int getWinSize(uint16_t *rows, uint16_t *columns)
{
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {return -1;}
        return cursorPosition(rows, columns);
    }
    else
    {
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// append buffer
struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int leng)
{
    char *new = realloc(ab->b, ab->len + leng);

    if(new == NULL){return;}
    memcpy(&new[ab->len], s, leng);
    ab->b = new;
    ab->len += leng;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

// output
void drawRows(struct abuf *ab)
{
    for(uint8_t i = 0; i < e.screen_rows; i++)
    {
        abAppend(ab, "~", 1);
        if(i < e.screen_rows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void refreshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    drawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
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