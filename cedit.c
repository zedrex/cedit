#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#define ctrl(key) (key & 0x1f)

struct editorAttributes
{
    int screenRow;
    int screenColumn;
    int cursorX;
    int cursorY;
    struct termios terminalDefault;
};

struct editorAttributes editor;

// Functions
void rawModeOff();
void rawModeOn();
void terminateProgram(char errorMessage[]);
char readCharacter();
void processInput();
void refreshEditor();
void standardExit();
void loadEditor();
int getWindowSize(int *rows, int *columns);
void startEditor();

// Exits after returning cursor to the front
void standardExit()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
}

//  Prints lines of tildes
void loadEditor()
{
    for (int i = 1; i <= editor.screenRow; i++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}
void rawModeOn()
{
    if (tcgetattr(STDIN_FILENO, &editor.terminalDefault) == -1)
    {
        terminateProgram("tcgetattr");
    }

    //Get Terminal Attributes

    struct termios rawMode = editor.terminalDefault;

    //Input flags
    rawMode.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    /*
    c_iflag : Input Flags
    Turning off:
    IXON - Ctrl+S and Ctrl+Q (input transmission shortcuts)
    ICRNL - Ctrl+M (carriage return)
    BRKINT - Ctrl+C (terminate program)
    INPCK - Parity Checking
    ISTRIP - Strip 8th bit
     */

    //Output Flags
    rawMode.c_oflag &= ~(OPOST);
    /*
    c_oflag : Output Flags
    Turning off:
    OPOST - Post processing of Output (Carriage return and newline formatting)
     */

    //Local Flags
    rawMode.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    /*
    c_lflag : Local Flags
    Turning off:
    ECHO - Echo to the terminal
    ICANON - Canonical mode (Turning off causes keypresses to be processed one by one)
    ISIG - Ctrl+C, Ctrl+Z
    IEXTEN - Ctrl+V
     */

    //Control Flags
    rawMode.c_cflag |= ~(CS8);
    /*
    c_cflag : Control Flags
    Turning on:
    CS8 - Set 8 bits per byte
     */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawMode) == -1)
    {
        terminateProgram("Tcsetattr error");
    }
    //Set Attribute when TCSAFLUSH (when all pending inputs have been processed)

    atexit(rawModeOff);
}

void startEditor()
{
    if (getWindowSize(&editor.screenRow, &editor.screenColumn) == -1)
    {
        terminateProgram("Window Size Error");
    }
}
char readCharacter()
{
    int readReturn;
    char character;
    while ((readReturn = read(STDIN_FILENO, &character, 1)) != 1)
    {
        if (readReturn == -1)
        {
            terminateProgram("Read Error");
        }
    }
    return character;
}

int currentCursorPosition(int *rows, int *columns)
{
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    {
        return -1;
    }
    printf("\r\n");
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1)
    {
        if (iscntrl(c))
        {
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d ('%c')\r\n", c, c);
        }
    }
    readCharacter();
    return -1;
}
void processInput()
{
    char character;
    character = readCharacter();
    if ((character == ctrl('q')) || (character == ctrl('Q')))
    {
        standardExit();
    }
}

void rawModeOff()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.terminalDefault) == -1)
    {
        terminateProgram("tcsetattr");
    }
}

void terminateProgram(char errorMessage[]) // Exit with Error
{
    refreshEditor();
    perror(errorMessage);
    exit(1);
}

int getWindowSize(int *rows, int *columns)
{
    struct winsize windowSize;
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == -1 || windowSize.ws_col == 0) //Loop cursor to bottom-right and calculate row and column size
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        {
            return -1;
        }
        return currentCursorPosition(rows, columns);
    }
    else
    {
        *columns = windowSize.ws_col;
        *rows = windowSize.ws_row;
        return 0;
    }
}
/*
void showFile(FILE *fp)
{
    refreshEditor();
    char c;
    do
    {
        c = getc(fp);
        char l[1];
        l[0] = c;
        if (c != EOF)
            write(STDOUT_FILENO, l, sizeof(l));
    } while (c != EOF);
}
*/
void refreshEditor()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    loadEditor();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

int main()
{
    rawModeOn();
    startEditor();

    while (1)
    {
        refreshEditor();
        processInput();
    }

    return 0;
}
