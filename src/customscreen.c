
//THIS FILE IS A BODGE, MANY CRAPPY PRACTISES WERE DONE.
//Not too sure on how to update the display reliably

#include "dryos.h"
#include "bmp.h"
#define MAXLINES 8
extern void uart_printf(const char *fmt, ...);

extern void font_draw(uint32_t, uint32_t, uint32_t, uint32_t, char *);

static int beforePosition;
static int currentPosition;

static int stack[MAXLINES + 1];
static int top = -1;

static int isempty()
{

    if (top == -1)
        return 1;
    else
        return 0;
}

static int isfull()
{

    if (top == MAXLINES)
        return 1;
    else
        return 0;
}

static int peek(int v)
{
    return stack[v];
}

static int pop()
{
    int data;

    if (!isempty())
    {
        data = stack[top];
        top = top - 1;
        return data;
    }
    return -1;
}

static int push(int data)
{

    if (!isfull())
    {
        top = top + 1;
        stack[top] = data;
    }
    else
    {
        pop();
        top = top + 1;
        stack[top] = data;
    }
}
static void insertAtBottom(int item)
{
    if (isempty())
    {
        push(item);
    }
    else
    {

        /* Store the top most element of stack in top variable and 
        recursively call insertAtBottom for rest of the stack */
        int top = pop();
        insertAtBottom(item);

        /* Once the item is inserted at the bottom, push the 
        top element back to stack */
        push(top);
    }
}

static struct stringqueue
{
    uint8_t msg[128];
} display[100];
int bmp_printf_auto(const char *fmt, ...)
{
    va_list ap;

    char bmp_printf_buf[128];
    va_start(ap, fmt);
    vsnprintf(bmp_printf_buf, sizeof(bmp_printf_buf) - 1, fmt, ap);
    va_end(ap);

    //int xx = x;
    //int yy = y;
    //font_drawf(vrambuffer, x, y, 0xff000000, 3, bmp_printf_buf, (marv->height), (marv->width));
    currentPosition++;
    if (currentPosition > MAXLINES)
    {
        int pops = pop();
        uart_printf("Popped! (%x) %s", pops, display[pops].msg);
        currentPosition = pops;
    }
    strcpy(display[currentPosition].msg, bmp_printf_buf);
    uart_printf("Pushing %s to stack index: %d\n", bmp_printf_buf, currentPosition);
    insertAtBottom(currentPosition);

    beforePosition = currentPosition;

    return 0;
}

static int x = 0;
static int y = 0;
void screen()
{
    while (!bmp_vram_raw())
    {
        msleep(100);
    }

    while (1)
    {
        if (MEM(0xfd94) != 0)
        {

            y = 0;
            int prev;
            //this refreshes the display. bodge.
            ((void (*)(int param_1, int x, int y, int w, int h))0xE04AFF41)(0, 0, 0, 10, 10);
            for (int index = currentPosition; index >= 0; index--)
            {

                if (prev == peek(index))
                {
                    continue;
                }
                //yes this draws pixel by pixel. good enough for PoC?
                font_draw(x, y, 0xFFFFFFF0, 3, display[peek(index)].msg);
                y += 30;

                prev = peek(index);
            }
        }
        msleep(10);
    }
}