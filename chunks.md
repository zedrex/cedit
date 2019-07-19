## Raw input mode

```c
#include<stdio.h>
#include<termios.h>
#include<unistd.h>

void disable_raw_mode(e_context* ctx) 
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx->orig) == -1) e_die("tcsetattr");
}


void enable_raw_mode(e_context* ctx) 
{
  struct termios raw;

  raw = ctx->orig;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= CS8;
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) e_die("tcsetattr");
}
```
