#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ColorPrint.h"

typedef struct color_tbl {
    color_t index;
    int number;
    int bold;
} color_tbl_t;

color_tbl_t ctbl[] = {
    {
        index : color_normal,
        number : 39,
        bold : 0,
    },
    {
        index : color_red,
        number : 31,
        bold : 1,
    },
    {
        index : color_blue,
        number : 34,
        bold : 1,
    },
    {
        index : color_green,
        number : 32,
        bold : 1,
    },
    {
        index : color_yellow,
        number : 33,
        bold : 1,
    },
};
static char color_token_buff[256];
static char buff[4096];

char *getColorToken(color_t color)
{
    if (color < 0 || color >= color_max)
        return NULL;
    sprintf(color_token_buff, "\033[%d;%dm", ctbl[color].bold, ctbl[color].number);
    return color_token_buff;
}

int sprintf_color(color_t color, char *colorStr, char *fmt, ...)
{
    va_list ap;

    char *colorToken = getColorToken(color);
    if (!colorToken)
        colorToken = getColorToken(color_red);

    char *tmp = colorStr;
    tmp += sprintf(tmp, "%s", colorToken);

    va_start(ap, fmt);
    vsprintf(buff, fmt, ap);
    strcat(tmp, buff);
    tmp += strlen(tmp);
    va_end(ap);

    strcat(tmp, getColorToken(color_normal));
        return strlen(colorStr);
}

void fprintf_color(color_t color, FILE *f, char *fmt, ...)
{
    va_list ap;

    char *colorToken = getColorToken(color);
    if (!colorToken)
        colorToken = getColorToken(color_red);

    fprintf(f, "%s", colorToken);

    va_start(ap, fmt);
    vsprintf(buff, fmt, ap);
    fprintf(f, "%s", buff);
    va_end(ap);

    fprintf(f, "%s", getColorToken(color_normal));
}
#ifdef __TEST__

int main()
{
    printf("---- Testing fprintf_color -----\n");
    fprintf_color(color_green, stdout, "BLABLA %s \n", "glagla");
    fprintf_color(color_blue, stdout, "BLABLA %s \n", "glagla");
    fprintf_color(color_yellow, stdout, "BLABLA %s \n", "glagla");
    fprintf_color(color_red, stdout, "BLABLA %s \n", "glagla");

    printf("---- Testing fprintf_xxx ----\n");
    fprintf_green(stdout, "GREEN GREEN\n");
    fprintf_blue(stdout, "GREEN GREEN\n");
    fprintf_yellow(stdout, "GREEN GREEN\n");
    fprintf_red(stdout, "GREEN GREEN\n");

    printf("---- Testing printf_xxx ----\n");
    fprintf_green(stdout, "GREEN GREEN\n");
    fprintf_blue(stdout, "GREEN GREEN\n");
    fprintf_yellow(stdout, "GREEN GREEN\n");
    fprintf_red(stdout, "GREEN GREEN\n");


    return 0;
}
#endif
