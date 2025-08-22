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
#define TEXT_EDITOR_VERSION "MILO --version 0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editor_key
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

// data
struct config
{
    uint16_t cx, cy;
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

int32_t editorReadKey()
{
    int32_t nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN) {die("read");}
    }

    if(c == '\x1b')
    {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1)!= 1) {return '\x1b';}
        if(read(STDIN_FILENO, &seq[1], 1)!= 1) {return '\x1b';}

        if(seq[0] == '[')
        {
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                if(read(STDIN_FILENO, &seq[2], 1)!= 1) {return '\x1b';}
                if(seq[2] == '~')
                {
                    switch(seq[1])
                    {
                        case '5':
                            return PAGE_UP;
                            break;
                        case '6':
                            return PAGE_DOWN;
                            break;
                    }
                }
            }
            else
            {
                switch(seq[1])
                {
                    case 'A':
                        return ARROW_UP;
                        break;
                    case 'B':
                        return ARROW_DOWN;
                        break;
                    case 'C':
                        return ARROW_RIGHT;
                        break;
                    case 'D':
                        return ARROW_LEFT;
                        break;
                }
            }
        }

        return '\x1b';
    }
    else {return c;}
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
    if(sscanf(&buf[2], "%hu;%hu", rows, columns) != 2) {return -1;}

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
        if(i == e.screen_rows/3)
        {
            char welcome[80];
            uint16_t welcomelen = snprintf(welcome, sizeof(welcome),
                "%s", TEXT_EDITOR_VERSION);

            if(welcomelen > e.screen_columns)
            {
                welcomelen = e.screen_columns;
            }

            uint16_t padding = (e.screen_columns - welcomelen) / 2;

            if(padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while(padding--)
            {
                abAppend(ab, " ", 1);
            }

            abAppend(ab, welcome, welcomelen);
        }
        else
        {
        abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if(i < e.screen_rows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void refreshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
 //   abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    drawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e.cy +1, e.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// input
void moveCursor(int32_t key)
{
    switch(key)
    {
        case ARROW_LEFT:
            if(e.cx != 0){e.cx--;}
            break;
        case ARROW_RIGHT:
            if(e.cx != e.screen_columns - 1){e.cx++;}
            break;
        case ARROW_UP:
            if(e.cy != 0){e.cy--;}
            break;
        case ARROW_DOWN:
            if(e.cy != e.screen_rows - 1){e.cy++;}
            break;
    }
}

void processKeypress()
{
    int32_t c = editorReadKey();
    
    printf("%d (%c)",c,c);
    switch(c)
    {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case PAGE_UP:
            {
                int16_t time = e.screen_rows;
                while(time--)
                {
                    moveCursor(ARROW_UP);
                }
                break;
            }
        case PAGE_DOWN:
            {
                int16_t time = e.screen_rows;
                while(time--)
                {
                    moveCursor(ARROW_DOWN);
                }
                break;
            }
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
    }
}

// init
void initEditor()
{
    e.cx = 0;
    e.cy = 0;
    if(getWinSize(&e.screen_rows, &e.screen_columns) == -1){die("getWinSize");}
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