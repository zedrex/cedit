/*** FEATURE TEST MACROS ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** DEFINITIONS ***/

#define CEDIT_VERSION "1.0"
#define CEDIT_TAB_STOP 8
#define CEDIT_QUIT_COUNT 2
#define ctrl(key) ((key)&0x1f)
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** GLOBAL DECLARATIONS ***/

enum ceditKey
{
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum ceditSyntaxHighlight
{
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

struct bufferContainer
{
  char *b;
  int length;
};

struct ceditSyntax
{
  char *fileType;
  char **fileMatch;
  char **keywords;
  char *singleLineCommentStart;
  char *multiLineCommentStart;
  char *multiLineCommentEnd;
  int flags;
};

typedef struct editorRow
{
  int index;
  int size;
  int rSize;
  int hlOpenComment;
  char *characters;
  char *render;
  unsigned char *hl;

} editorRow;

struct ceditConfig
{
  int cursorX, cursorY;
  int rowX;
  int rowOff;
  int columnOff;
  int terminalRows;
  int terminalColumns;
  int rowNum;
  int modified;
  char *fileName;
  char statusMessage[80];
  time_t statusMessageTime;
  struct ceditSyntax *syntax;
  struct termios terminalDefault;
  editorRow *row;
} Cedit;

/*** SYNTAX DEFINITIONS ***/
char *extensionC[] = {".character", ".h", ".cpp", NULL};
char *keywordsC[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL};

struct ceditSyntax HLDB[] = {
    {"character",
     extensionC,
     keywordsC,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** FUNCTION PROTOTYPES ***/

void startCedit();
void terminateProgram(const char *errorMessage);
void rawModeOff();
void rawModeOn();
void ceditUpdateSyntax(editorRow *row);
void ceditHighlightSyntax();
void ceditUpdateRow(editorRow *row);
void ceditInsertRow(int at, char *s, size_t length);
void ceditFreeRow(editorRow *row);
void ceditDeleteRow(int at);
void ceditRowInsertCharacter(editorRow *row, int at, int character);
void ceditRowAppendString(editorRow *row, char *s, size_t length);
void ceditRowDeleteCharacter(editorRow *row, int at);
void ceditInsertCharacter(int character);
void ceditInsertNewline();
void ceditDeleteCharacter();
void ceditOpen(char *fileName);
void ceditSave();
void ceditFindCallback(char *query, int key);
void ceditFind();
void appendBuffer(struct bufferContainer *bc, const char *s, int length);
void freeBuffer(struct bufferContainer *bc);
void ceditScroll();
void ceditPrintRows(struct bufferContainer *bc);
void ceditDrawStatusBar(struct bufferContainer *bc);
void ceditDrawMessageBar(struct bufferContainer *bc);
void ceditRefreshTerminal();
void ceditSetStatusMessage(const char *fmt, ...);
void ceditMoveCursor(int key);
void ceditProcessKeypress();
int ceditReadCharacter();
int getCursorPosition(int *rows, int *columns);
int ceditRowCursorTransformCxtoRx(editorRow *row, int cursorX);
int ceditRowCursorTransformRxToCx(editorRow *row, int rowX);
int getTerminalSize(int *rows, int *columns);
int isSeparator(int character);
int ceditSyntaxColoring(int hl);
char *ceditPrompt(char *prompt, void (*callback)(char *, int));
char *ceditRowToString(int *bufferLength);

/*** TERMINAL MANIPULATION ***/

void terminateProgram(const char *errorMessage)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(errorMessage);
  write(STDOUT_FILENO, "\n\r", 2);
  exit(1);
}

void usageProgram()
{
  char msg[] = "Cedit Usage:\n\rOpen new file:\t\t./cedit\n\rEdit existing file:\t./cedit filename\n\r";
  write(STDOUT_FILENO, msg, sizeof(msg));
  exit(1);
}

void rawModeOff()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Cedit.terminalDefault) == -1)
    terminateProgram("Tcsetattr Error!");
}

void rawModeOn()
{
  if (tcgetattr(STDIN_FILENO, &Cedit.terminalDefault) == -1)
    terminateProgram("Tcgetattr Error!");
  atexit(rawModeOff);

  struct termios rawMode = Cedit.terminalDefault;

  //Input flags
  rawMode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
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

  rawMode.c_cc[VMIN] = 0;
  rawMode.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawMode) == -1)
    terminateProgram("Tcsetattr Error!");
}

int ceditReadCharacter()
{
  int readReturn;
  char character;
  while ((readReturn = read(STDIN_FILENO, &character, 1)) != 1)
  {
    if (readReturn == -1 && errno != EAGAIN)
      terminateProgram("Read Error!");
  }

  if (character == '\x1b')
  {
    char escapeSequence[3];

    if (read(STDIN_FILENO, &escapeSequence[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &escapeSequence[1], 1) != 1)
      return '\x1b';

    if (escapeSequence[0] == '[')
    {
      if (escapeSequence[1] >= '0' && escapeSequence[1] <= '9')
      {
        if (read(STDIN_FILENO, &escapeSequence[2], 1) != 1)
          return '\x1b';
        if (escapeSequence[2] == '~')
        {
          switch (escapeSequence[1])
          {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      }
      else
      {
        switch (escapeSequence[1])
        {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    }
    else if (escapeSequence[0] == 'O')
    {
      switch (escapeSequence[1])
      {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  }
  else
  {
    return character;
  }
}

int getCursorPosition(int *rows, int *columns)
{
  char buffer[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buffer) - 1)
  {
    if (read(STDIN_FILENO, &buffer[i], 1) != 1)
      break;
    if (buffer[i] == 'R')
      break;
    i++;
  }
  buffer[i] = '\0';

  if (buffer[0] != '\x1b' || buffer[1] != '[')
    return -1;
  if (sscanf(&buffer[2], "%d;%d", rows, columns) != 2)
    return -1;

  return 0;
}

int getTerminalSize(int *rows, int *columns)
{
  struct winsize terminalSize;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminalSize) == -1 || terminalSize.ws_col == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, columns);
  }
  else
  {
    *columns = terminalSize.ws_col;
    *rows = terminalSize.ws_row;
    return 0;
  }
}

/*** SYNTAX HIGHLIGHTING ***/

int isSeparator(int character)
{
  return isspace(character) || character == '\0' || strchr(",.()+-/*=~%<>[];", character) != NULL;
}

void ceditUpdateSyntax(editorRow *row)
{
  row->hl = realloc(row->hl, row->rSize);
  memset(row->hl, HL_NORMAL, row->rSize);

  if (Cedit.syntax == NULL)
    return;

  char **keywords = Cedit.syntax->keywords;

  char *scs = Cedit.syntax->singleLineCommentStart;
  char *mcs = Cedit.syntax->multiLineCommentStart;
  char *mce = Cedit.syntax->multiLineCommentEnd;

  int scsLength = scs ? strlen(scs) : 0;
  int mcsLength = mcs ? strlen(mcs) : 0;
  int mceLength = mce ? strlen(mce) : 0;

  int prevSep = 1;
  int inString = 0;
  int inComment = (row->index > 0 && Cedit.row[row->index - 1].hlOpenComment);

  int i = 0;
  while (i < row->rSize)
  {
    char character = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scsLength && !inString && !inComment)
    {
      if (!strncmp(&row->render[i], scs, scsLength))
      {
        memset(&row->hl[i], HL_COMMENT, row->rSize - i);
        break;
      }
    }

    if (mcsLength && mceLength && !inString)
    {
      if (inComment)
      {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mceLength))
        {
          memset(&row->hl[i], HL_MLCOMMENT, mceLength);
          i += mceLength;
          inComment = 0;
          prevSep = 1;
          continue;
        }
        else
        {
          i++;
          continue;
        }
      }
      else if (!strncmp(&row->render[i], mcs, mcsLength))
      {
        memset(&row->hl[i], HL_MLCOMMENT, mcsLength);
        i += mcsLength;
        inComment = 1;
        continue;
      }
    }

    if (Cedit.syntax->flags & HL_HIGHLIGHT_STRINGS)
    {
      if (inString)
      {
        row->hl[i] = HL_STRING;
        if (character == '\\' && i + 1 < row->rSize)
        {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (character == inString)
          inString = 0;
        i++;
        prevSep = 1;
        continue;
      }
      else
      {
        if (character == '"' || character == '\'')
        {
          inString = character;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (Cedit.syntax->flags & HL_HIGHLIGHT_NUMBERS)
    {
      if ((isdigit(character) && (prevSep || prev_hl == HL_NUMBER)) ||
          (character == '.' && prev_hl == HL_NUMBER))
      {
        row->hl[i] = HL_NUMBER;
        i++;
        prevSep = 0;
        continue;
      }
    }

    if (prevSep)
    {
      int j;
      for (j = 0; keywords[j]; j++)
      {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;

        if (!strncmp(&row->render[i], keywords[j], klen) &&
            isSeparator(row->render[i + klen]))
        {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL)
      {
        prevSep = 0;
        continue;
      }
    }

    prevSep = isSeparator(character);
    i++;
  }

  int changed = (row->hlOpenComment != inComment);
  row->hlOpenComment = inComment;
  if (changed && row->index + 1 < Cedit.rowNum)
    ceditUpdateSyntax(&Cedit.row[row->index + 1]);
}

int ceditSyntaxColoring(int hl)
{
  switch (hl)
  {
  case HL_COMMENT:
  case HL_MLCOMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
  case HL_MATCH:
    return 34;
  default:
    return 37;
  }
}

void ceditHighlightSyntax()
{
  Cedit.syntax = NULL;
  if (Cedit.fileName == NULL)
    return;

  char *ext = strrchr(Cedit.fileName, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++)
  {
    struct ceditSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->fileMatch[i])
    {
      int isExtension = (s->fileMatch[i][0] == '.');
      if ((isExtension && ext && !strcmp(ext, s->fileMatch[i])) ||
          (!isExtension && strstr(Cedit.fileName, s->fileMatch[i])))
      {
        Cedit.syntax = s;

        int fileRow;
        for (fileRow = 0; fileRow < Cedit.rowNum; fileRow++)
        {
          ceditUpdateSyntax(&Cedit.row[fileRow]);
        }

        return;
      }
      i++;
    }
  }
}

/*** ROW OPERATIONS ***/

int ceditRowCursorTransformCxtoRx(editorRow *row, int cursorX)
{
  int rowX = 0;
  int j;
  for (j = 0; j < cursorX; j++)
  {
    if (row->characters[j] == '\t')
      rowX += (CEDIT_TAB_STOP - 1) - (rowX % CEDIT_TAB_STOP);
    rowX++;
  }
  return rowX;
}

int ceditRowCursorTransformRxToCx(editorRow *row, int rowX)
{
  int currentRx = 0;
  int cursorX;
  for (cursorX = 0; cursorX < row->size; cursorX++)
  {
    if (row->characters[cursorX] == '\t')
      currentRx += (CEDIT_TAB_STOP - 1) - (currentRx % CEDIT_TAB_STOP);
    currentRx++;

    if (currentRx > rowX)
      return cursorX;
  }
  return cursorX;
}

void ceditUpdateRow(editorRow *row)
{
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->characters[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (CEDIT_TAB_STOP - 1) + 1);

  int index = 0;
  for (j = 0; j < row->size; j++)
  {
    if (row->characters[j] == '\t')
    {
      row->render[index++] = ' ';
      while (index % CEDIT_TAB_STOP != 0)
        row->render[index++] = ' ';
    }
    else
    {
      row->render[index++] = row->characters[j];
    }
  }
  row->render[index] = '\0';
  row->rSize = index;

  ceditUpdateSyntax(row);
}

void ceditInsertRow(int at, char *s, size_t length)
{
  if (at < 0 || at > Cedit.rowNum)
    return;

  Cedit.row = realloc(Cedit.row, sizeof(editorRow) * (Cedit.rowNum + 1));
  memmove(&Cedit.row[at + 1], &Cedit.row[at], sizeof(editorRow) * (Cedit.rowNum - at));
  for (int j = at + 1; j <= Cedit.rowNum; j++)
    Cedit.row[j].index++;

  Cedit.row[at].index = at;

  Cedit.row[at].size = length;
  Cedit.row[at].characters = malloc(length + 1);
  memcpy(Cedit.row[at].characters, s, length);
  Cedit.row[at].characters[length] = '\0';

  Cedit.row[at].rSize = 0;
  Cedit.row[at].render = NULL;
  Cedit.row[at].hl = NULL;
  Cedit.row[at].hlOpenComment = 0;
  ceditUpdateRow(&Cedit.row[at]);

  Cedit.rowNum++;
  Cedit.modified++;
}

void ceditFreeRow(editorRow *row)
{
  free(row->render);
  free(row->characters);
  free(row->hl);
}

void ceditDeleteRow(int at)
{
  if (at < 0 || at >= Cedit.rowNum)
    return;
  ceditFreeRow(&Cedit.row[at]);
  memmove(&Cedit.row[at], &Cedit.row[at + 1], sizeof(editorRow) * (Cedit.rowNum - at - 1));
  for (int j = at; j < Cedit.rowNum - 1; j++)
    Cedit.row[j].index--;
  Cedit.rowNum--;
  Cedit.modified++;
}

void ceditRowInsertCharacter(editorRow *row, int at, int character)
{
  if (at < 0 || at > row->size)
    at = row->size;
  row->characters = realloc(row->characters, row->size + 2);
  memmove(&row->characters[at + 1], &row->characters[at], row->size - at + 1);
  row->size++;
  row->characters[at] = character;
  ceditUpdateRow(row);
  Cedit.modified++;
}

void ceditRowAppendString(editorRow *row, char *s, size_t length)
{
  row->characters = realloc(row->characters, row->size + length + 1);
  memcpy(&row->characters[row->size], s, length);
  row->size += length;
  row->characters[row->size] = '\0';
  ceditUpdateRow(row);
  Cedit.modified++;
}

void ceditRowDeleteCharacter(editorRow *row, int at)
{
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->characters[at], &row->characters[at + 1], row->size - at);
  row->size--;
  ceditUpdateRow(row);
  Cedit.modified++;
}

/*** CEDIT OPERATIONS ***/

void ceditInsertCharacter(int character)
{
  if (Cedit.cursorY == Cedit.rowNum)
  {
    ceditInsertRow(Cedit.rowNum, "", 0);
  }
  ceditRowInsertCharacter(&Cedit.row[Cedit.cursorY], Cedit.cursorX, character);
  Cedit.cursorX++;
}

void ceditInsertNewline()
{
  if (Cedit.cursorX == 0)
  {
    ceditInsertRow(Cedit.cursorY, "", 0);
  }
  else
  {
    editorRow *row = &Cedit.row[Cedit.cursorY];
    ceditInsertRow(Cedit.cursorY + 1, &row->characters[Cedit.cursorX], row->size - Cedit.cursorX);
    row = &Cedit.row[Cedit.cursorY];
    row->size = Cedit.cursorX;
    row->characters[row->size] = '\0';
    ceditUpdateRow(row);
  }
  Cedit.cursorY++;
  Cedit.cursorX = 0;
}

void ceditDeleteCharacter()
{
  if (Cedit.cursorY == Cedit.rowNum)
    return;
  if (Cedit.cursorX == 0 && Cedit.cursorY == 0)
    return;

  editorRow *row = &Cedit.row[Cedit.cursorY];
  if (Cedit.cursorX > 0)
  {
    ceditRowDeleteCharacter(row, Cedit.cursorX - 1);
    Cedit.cursorX--;
  }
  else
  {
    Cedit.cursorX = Cedit.row[Cedit.cursorY - 1].size;
    ceditRowAppendString(&Cedit.row[Cedit.cursorY - 1], row->characters, row->size);
    ceditDeleteRow(Cedit.cursorY);
    Cedit.cursorY--;
  }
}

/*** FILE OPERATIONS ***/

char *ceditRowToString(int *bufferLength)
{
  int totalLength = 0;
  int j;
  for (j = 0; j < Cedit.rowNum; j++)
    totalLength += Cedit.row[j].size + 1;
  *bufferLength = totalLength;

  char *buffer = malloc(totalLength);
  char *p = buffer;
  for (j = 0; j < Cedit.rowNum; j++)
  {
    memcpy(p, Cedit.row[j].characters, Cedit.row[j].size);
    p += Cedit.row[j].size;
    *p = '\n';
    p++;
  }

  return buffer;
}

void ceditOpen(char *fileName)
{
  free(Cedit.fileName);
  Cedit.fileName = strdup(fileName);

  ceditHighlightSyntax();

  FILE *fp = fopen(fileName, "r");
  if (!fp)
    terminateProgram("File Open Error!");

  char *line = NULL;
  size_t lineCap = 0;
  ssize_t lineLength;
  while ((lineLength = getline(&line, &lineCap, fp)) != -1)
  {
    while (lineLength > 0 && (line[lineLength - 1] == '\n' ||
                              line[lineLength - 1] == '\r'))
      lineLength--;
    ceditInsertRow(Cedit.rowNum, line, lineLength);
  }
  free(line);
  fclose(fp);
  Cedit.modified = 0;
}

void ceditSave()
{
  if (Cedit.fileName == NULL)
  {
    Cedit.fileName = ceditPrompt("Save as: %s (ESC to cancel)", NULL);
    if (Cedit.fileName == NULL)
    {
      ceditSetStatusMessage("Save aborted");
      return;
    }
    ceditHighlightSyntax();
  }

  int length;
  char *buffer = ceditRowToString(&length);

  int fd = open(Cedit.fileName, O_RDWR | O_CREAT, 0644);
  if (fd != -1)
  {
    if (ftruncate(fd, length) != -1)
    {
      if (write(fd, buffer, length) == length)
      {
        close(fd);
        free(buffer);
        Cedit.modified = 0;
        ceditSetStatusMessage("%d bytes written to disk", length);
        return;
      }
    }
    close(fd);
  }

  free(buffer);
  ceditSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** FIND OPERATIONS ***/

void ceditFindCallback(char *query, int key)
{
  static int lastMatch = -1;
  static int direction = 1;

  static int savedHlLine;
  static char *savedHl = NULL;

  if (savedHl)
  {
    memcpy(Cedit.row[savedHlLine].hl, savedHl, Cedit.row[savedHlLine].rSize);
    free(savedHl);
    savedHl = NULL;
  }

  if (key == '\r' || key == '\x1b')
  {
    lastMatch = -1;
    direction = 1;
    return;
  }
  else if (key == ARROW_RIGHT || key == ARROW_DOWN)
  {
    direction = 1;
  }
  else if (key == ARROW_LEFT || key == ARROW_UP)
  {
    direction = -1;
  }
  else
  {
    lastMatch = -1;
    direction = 1;
  }

  if (lastMatch == -1)
    direction = 1;
  int current = lastMatch;
  int i;
  for (i = 0; i < Cedit.rowNum; i++)
  {
    current += direction;
    if (current == -1)
      current = Cedit.rowNum - 1;
    else if (current == Cedit.rowNum)
      current = 0;

    editorRow *row = &Cedit.row[current];
    char *match = strstr(row->render, query);
    if (match)
    {
      lastMatch = current;
      Cedit.cursorY = current;
      Cedit.cursorX = ceditRowCursorTransformRxToCx(row, match - row->render);
      Cedit.rowOff = Cedit.rowNum;

      savedHlLine = current;
      savedHl = malloc(row->rSize);
      memcpy(savedHl, row->hl, row->rSize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void ceditFind()
{
  int savedCursorX = Cedit.cursorX;
  int savedCursorY = Cedit.cursorY;
  int savedColumn = Cedit.columnOff;
  int savedRow = Cedit.rowOff;

  char *query = ceditPrompt("Search: %s (Use ESC/Arrows/Enter)",
                            ceditFindCallback);

  if (query)
  {
    free(query);
  }
  else
  {
    Cedit.cursorX = savedCursorX;
    Cedit.cursorY = savedCursorY;
    Cedit.columnOff = savedColumn;
    Cedit.rowOff = savedRow;
  }
}

/*** BUFFER FUNCTIONS ***/

#define BUFFER_INITIALIZATION \
  {                           \
    NULL, 0                   \
  }

void appendBuffer(struct bufferContainer *bc, const char *s, int length)
{
  char *new = realloc(bc->b, bc->length + length);

  if (new == NULL)
    return;
  memcpy(&new[bc->length], s, length);
  bc->b = new;
  bc->length += length;
}

void freeBuffer(struct bufferContainer *bc)
{
  free(bc->b);
}

/*** OUTPUT OPERATIONS ***/

void ceditScroll()
{
  Cedit.rowX = 0;
  if (Cedit.cursorY < Cedit.rowNum)
  {
    Cedit.rowX = ceditRowCursorTransformCxtoRx(&Cedit.row[Cedit.cursorY], Cedit.cursorX);
  }
  if (Cedit.cursorY < Cedit.rowOff)
  {
    Cedit.rowOff = Cedit.cursorY;
  }
  if (Cedit.cursorY >= Cedit.rowOff + Cedit.terminalRows)
  {
    Cedit.rowOff = Cedit.cursorY - Cedit.terminalRows + 1;
  }
  if (Cedit.rowX < Cedit.columnOff)
  {
    Cedit.columnOff = Cedit.rowX;
  }
  if (Cedit.rowX >= Cedit.columnOff + Cedit.terminalColumns)
  {
    Cedit.columnOff = Cedit.rowX - Cedit.terminalColumns + 1;
  }
}

void ceditPrintRows(struct bufferContainer *bc)
{
  int y;
  for (y = 0; y < Cedit.terminalRows; y++)
  {
    int fileRow = y + Cedit.rowOff;
    if (fileRow >= Cedit.rowNum)
    {
      if (Cedit.rowNum == 0 && y == Cedit.terminalRows / 3)
      {
        char welcomeMessage[80];
        int welcomeLength = snprintf(welcomeMessage, sizeof(welcomeMessage),
                                     "Cedit - v%s", CEDIT_VERSION);
        if (welcomeLength > Cedit.terminalColumns)
          welcomeLength = Cedit.terminalColumns;
        int padding = (Cedit.terminalColumns - welcomeLength) / 2;
        if (padding)
        {
          appendBuffer(bc, "~", 1);
          padding--;
        }
        while (padding--)
          appendBuffer(bc, " ", 1);
        appendBuffer(bc, welcomeMessage, welcomeLength);
      }
      else
      {
        appendBuffer(bc, "~", 1);
      }
    }
    else
    {
      int length = Cedit.row[fileRow].rSize - Cedit.columnOff;
      if (length < 0)
        length = 0;
      if (length > Cedit.terminalColumns)
        length = Cedit.terminalColumns;
      char *character = &Cedit.row[fileRow].render[Cedit.columnOff];
      unsigned char *hl = &Cedit.row[fileRow].hl[Cedit.columnOff];
      int currentColor = -1;
      int j;
      for (j = 0; j < length; j++)
      {
        if (iscntrl(character[j]))
        {
          char symbol = (character[j] <= 26) ? '@' + character[j] : '?';
          appendBuffer(bc, "\x1b[7m", 4);
          appendBuffer(bc, &symbol, 1);
          appendBuffer(bc, "\x1b[m", 3);
          if (currentColor != -1)
          {
            char buffer[16];
            int cLength = snprintf(buffer, sizeof(buffer), "\x1b[%dm", currentColor);
            appendBuffer(bc, buffer, cLength);
          }
        }
        else if (hl[j] == HL_NORMAL)
        {
          if (currentColor != -1)
          {
            appendBuffer(bc, "\x1b[39m", 5);
            currentColor = -1;
          }
          appendBuffer(bc, &character[j], 1);
        }
        else
        {
          int color = ceditSyntaxColoring(hl[j]);
          if (color != currentColor)
          {
            currentColor = color;
            char buffer[16];
            int cLength = snprintf(buffer, sizeof(buffer), "\x1b[%dm", color);
            appendBuffer(bc, buffer, cLength);
          }
          appendBuffer(bc, &character[j], 1);
        }
      }
      appendBuffer(bc, "\x1b[39m", 5);
    }

    appendBuffer(bc, "\x1b[K", 3);
    appendBuffer(bc, "\r\n", 2);
  }
}

void ceditDrawStatusBar(struct bufferContainer *bc)
{
  appendBuffer(bc, "\x1b[7m", 4);
  char status[80], rStatus[80];
  int length = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                        Cedit.fileName ? Cedit.fileName : "[No Name]", Cedit.rowNum,
                        Cedit.modified ? "(modified)" : "");
  int rLength = snprintf(rStatus, sizeof(rStatus), "%s | %d/%d",
                         Cedit.syntax ? Cedit.syntax->fileType : "Line number:", Cedit.cursorY + 1, Cedit.rowNum);
  if (length > Cedit.terminalColumns)
    length = Cedit.terminalColumns;
  appendBuffer(bc, status, length);
  while (length < Cedit.terminalColumns)
  {
    if (Cedit.terminalColumns - length == rLength)
    {
      appendBuffer(bc, rStatus, rLength);
      break;
    }
    else
    {
      appendBuffer(bc, " ", 1);
      length++;
    }
  }
  appendBuffer(bc, "\x1b[m", 3);
  appendBuffer(bc, "\r\n", 2);
}

void ceditDrawMessageBar(struct bufferContainer *bc)
{
  appendBuffer(bc, "\x1b[K", 3);
  int messageLength = strlen(Cedit.statusMessage);
  if (messageLength > Cedit.terminalColumns)
    messageLength = Cedit.terminalColumns;
  if (messageLength && time(NULL) - Cedit.statusMessageTime < 5)
    appendBuffer(bc, Cedit.statusMessage, messageLength);
}

void ceditRefreshTerminal()
{
  ceditScroll();

  struct bufferContainer bc = BUFFER_INITIALIZATION;

  appendBuffer(&bc, "\x1b[?25l", 6);
  appendBuffer(&bc, "\x1b[H", 3);

  ceditPrintRows(&bc);
  ceditDrawStatusBar(&bc);
  ceditDrawMessageBar(&bc);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (Cedit.cursorY - Cedit.rowOff) + 1,
           (Cedit.rowX - Cedit.columnOff) + 1);
  appendBuffer(&bc, buffer, strlen(buffer));

  appendBuffer(&bc, "\x1b[?25h", 6);

  write(STDOUT_FILENO, bc.b, bc.length);
  freeBuffer(&bc);
}

void ceditSetStatusMessage(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(Cedit.statusMessage, sizeof(Cedit.statusMessage), fmt, ap);
  va_end(ap);
  Cedit.statusMessageTime = time(NULL);
}

/*** INPUT OPERATIONS ***/

char *ceditPrompt(char *prompt, void (*callback)(char *, int))
{
  size_t bufferSize = 128;
  char *buffer = malloc(bufferSize);

  size_t bufferLength = 0;
  buffer[0] = '\0';

  while (1)
  {
    ceditSetStatusMessage(prompt, buffer);
    ceditRefreshTerminal();

    int character = ceditReadCharacter();
    if (character == DEL_KEY || character == ctrl('h') || character == BACKSPACE)
    {
      if (bufferLength != 0)
        buffer[--bufferLength] = '\0';
    }
    else if (character == '\x1b')
    {
      ceditSetStatusMessage("");
      if (callback)
        callback(buffer, character);
      free(buffer);
      return NULL;
    }
    else if (character == '\r')
    {
      if (bufferLength != 0)
      {
        ceditSetStatusMessage("");
        if (callback)
          callback(buffer, character);
        return buffer;
      }
    }
    else if (!iscntrl(character) && character < 128)
    {
      if (bufferLength == bufferSize - 1)
      {
        bufferSize *= 2;
        buffer = realloc(buffer, bufferSize);
      }
      buffer[bufferLength++] = character;
      buffer[bufferLength] = '\0';
    }

    if (callback)
      callback(buffer, character);
  }
}

void ceditMoveCursor(int key)
{
  editorRow *row = (Cedit.cursorY >= Cedit.rowNum) ? NULL : &Cedit.row[Cedit.cursorY];

  switch (key)
  {
  case ARROW_LEFT:
    if (Cedit.cursorX != 0)
    {
      Cedit.cursorX--;
    }
    else if (Cedit.cursorY > 0)
    {
      Cedit.cursorY--;
      Cedit.cursorX = Cedit.row[Cedit.cursorY].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && Cedit.cursorX < row->size)
    {
      Cedit.cursorX++;
    }
    else if (row && Cedit.cursorX == row->size)
    {
      Cedit.cursorY++;
      Cedit.cursorX = 0;
    }
    break;
  case ARROW_UP:
    if (Cedit.cursorY != 0)
    {
      Cedit.cursorY--;
    }
    break;
  case ARROW_DOWN:
    if (Cedit.cursorY < Cedit.rowNum)
    {
      Cedit.cursorY++;
    }
    break;
  }

  row = (Cedit.cursorY >= Cedit.rowNum) ? NULL : &Cedit.row[Cedit.cursorY];
  int rowLength = row ? row->size : 0;
  if (Cedit.cursorX > rowLength)
  {
    Cedit.cursorX = rowLength;
  }
}

void ceditProcessKeypress()
{
  static int quitCount = CEDIT_QUIT_COUNT;

  int character = ceditReadCharacter();

  switch (character)
  {
  case '\r':
    ceditInsertNewline();
    break;

  case ctrl('q'):
    if (Cedit.modified && quitCount > 0)
    {
      ceditSetStatusMessage("Warning! File has unsaved changes. "
                            "Press ctrl+Q %d more times to quit.",
                            quitCount);
      quitCount--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case ctrl('s'):
    ceditSave();
    break;

  case HOME_KEY:
    Cedit.cursorX = 0;
    break;

  case END_KEY:
    if (Cedit.cursorY < Cedit.rowNum)
      Cedit.cursorX = Cedit.row[Cedit.cursorY].size;
    break;

  case ctrl('f'):
    ceditFind();
    break;

  case BACKSPACE:
  case ctrl('h'):
  case DEL_KEY:
    if (character == DEL_KEY)
      ceditMoveCursor(ARROW_RIGHT);
    ceditDeleteCharacter();
    break;

  case PAGE_UP:
  case PAGE_DOWN:
  {
    if (character == PAGE_UP)
    {
      Cedit.cursorY = Cedit.rowOff;
    }
    else if (character == PAGE_DOWN)
    {
      Cedit.cursorY = Cedit.rowOff + Cedit.terminalRows - 1;
      if (Cedit.cursorY > Cedit.rowNum)
        Cedit.cursorY = Cedit.rowNum;
    }

    int times = Cedit.terminalRows;
    while (times--)
      ceditMoveCursor(character == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  }
  break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    ceditMoveCursor(character);
    break;

  case ctrl('l'):
  case '\x1b':
    break;

  default:
    ceditInsertCharacter(character);
    break;
  }

  quitCount = CEDIT_QUIT_COUNT;
}

/*** INITIALIZATION ***/

void startCedit()
{
  Cedit.cursorX = 0;
  Cedit.cursorY = 0;
  Cedit.rowX = 0;
  Cedit.rowOff = 0;
  Cedit.columnOff = 0;
  Cedit.rowNum = 0;
  Cedit.row = NULL;
  Cedit.modified = 0;
  Cedit.fileName = NULL;
  Cedit.statusMessage[0] = '\0';
  Cedit.statusMessageTime = 0;
  Cedit.syntax = NULL;

  if (getTerminalSize(&Cedit.terminalRows, &Cedit.terminalColumns) == -1)
    terminateProgram("Window Size Error!");
  Cedit.terminalRows -= 2;
}

/*** MAIN FUNCTION ***/
int main(int argc, char *argv[])
{
  rawModeOn();
  startCedit();
  if (argc == 2)
  {
    ceditOpen(argv[1]);
  }
  else if (argc != 1 && argc != 2)
  {
    usageProgram();
  }

  ceditSetStatusMessage("Use: ctrl+S = Save | ctrl+Q = Quit | ctrl+F = Find");

  while (1)
  {
    ceditRefreshTerminal();
    ceditProcessKeypress();
  }

  return 0;
}