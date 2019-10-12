#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#define ctrl(key) (key & 0x1f)

void rawModeOff();
void rawModeOn();
void terminateProgram(char errorMessage[]);
char readCharacter();
void processInput();
void standardExit();

struct termios terminalDefault;
//To contain the terminal attributes

void standardExit()
{
    exit(0);
}

void rawModeOn()
{
    if (tcgetattr(STDIN_FILENO, &terminalDefault) == -1)
    {
        terminateProgram("Tcgetattr error");
    }
    //Get Terminal Attributes

    struct termios rawMode = terminalDefault;

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

void processInput()
{
    char character;
    character = readCharacter();

    if (!((character == ctrl('q')) || (character == ctrl('Q'))))
    {
        printf("You pressed %c\r\n", character); //Key presses
    }
    else
    {
        standardExit();
    }
}

void rawModeOff()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminalDefault) == -1)
    {
        terminateProgram("Tcsetattr error");
    }
}

void terminateProgram(char errorMessage[])
{
    perror(errorMessage); //Show error message
    exit(1);
}

int main()
{
    rawModeOn();
    while (1)
    {
        processInput();
    }

    return 0;
}
