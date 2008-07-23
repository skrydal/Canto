#include <Python.h>
#include <py_curses.h>
#include <ncursesw/ncurses.h>

static int theme_strlen(char *message, char end)
{
    int len = 0;
    int i = 0;

    while ((message[i] != end) && (message[i] != 0)) {
        if (message[i] == '%') {
            i++;
        } else if (message[i] == '\\') {
            i++;
            len++;
        } else if ((unsigned char) message[i] > 0x7f) {
            wchar_t dest[2];
            i += mbtowc(dest, &message[i], 3) - 1;
            len += wcwidth(dest[0]);
        } else if (message[i] != '\n')
            len++;
        i++;
    }

    return len;
}

static PyObject *tlen(PyObject *self, PyObject *args)
{
    char *message;
    char end = 0;

    if(!PyArg_ParseTuple(args, "s|c", &message, &end))
            return NULL;

    return Py_BuildValue("i",theme_strlen(message, end));
}

static void style_box(WINDOW *win, char code)
{
    /* This function's limited memory */
    static int cur_colp = 1;
    static int prev_colp = 1;
    static char attrs[6] = {0,0,0,0,0,0};

    if (code == 'B') {
        attrs[0]++;
        if(!attrs[5])
            wattron(win, A_BOLD);
    }
    else if (code == 'b') {
        attrs[0]--;
        if(!attrs[0])
            wattroff(win, A_BOLD);
    }
    else if (code == 'U') {
        attrs[1]++;
        if(!attrs[5])  
            wattron(win, A_UNDERLINE);
    }
    else if (code == 'u') {
        attrs[1]--;
        if(!attrs[1])
            wattroff(win, A_UNDERLINE);
    }
    else if (code == 'S') {
        attrs[2]++;
        if(!attrs[5])
            wattron(win, A_STANDOUT);
    }
    else if (code == 's') {
        attrs[2]--;
        if(!attrs[2])
            wattroff(win, A_STANDOUT);
    }
    else if (code == 'R') {
        attrs[3]++;
        if(!attrs[5])
            wattron(win, A_REVERSE);
    }
    else if (code == 'r') {
        attrs[3]--;
        if(!attrs[3])
            wattroff(win, A_REVERSE);
    }
    else if (code == 'D') {
        attrs[4]++;
        if(!attrs[5])
            wattron(win, A_DIM);
    }
    else if (code == 'd') {
        attrs[4]--;
        if(!attrs[4])
            wattroff(win, A_DIM);
    }   
    /* For some reason wattron(win, A_NORMAL) doesn't work. */
    else if (code == 'N') {
        attrs[5]++;
        if (win)
            wattrset(win, 0);
    }
    else if (code == 'n') {
        attrs[5]--;
        if(!attrs[5]) {
            if(attrs[0])
                wattron(win, A_BOLD);
            if(attrs[1])
                wattron(win, A_UNDERLINE);
            if(attrs[2])
                wattron(win, A_STANDOUT);
            if(attrs[3])
                wattron(win, A_REVERSE);
            if(attrs[4])
                wattron(win, A_DIM);
        }
    }
    else if (code == 'C') {
        int j = 0;
        for(j = 0; j < 5; j++)
            attrs[j] = 0;
        if (win)
            wattrset(win, 0);
    }
    else if (code == '0') {
        cur_colp = prev_colp;
        wattron(win, COLOR_PAIR(cur_colp));
    }
    else if ((code >= '1') && (code <= '8')) {
        prev_colp = cur_colp;
        cur_colp = code - '0';
        wattron(win, COLOR_PAIR(cur_colp));
    }
}

static int putxy(WINDOW *win, int width, int *i, int *y, int *x, char *str)
{
    if ((unsigned char) str[0] > 0x7F) {
        wchar_t dest[2];
        int bytes = mbtowc(dest, &str[0], 3) - 1;

        if (bytes < 0)
            mvwaddch(win, *y, (*x)++, str[0]);
        else {
            /* To deal with non-latin characters that can take
               up more than one character's alotted width, 
               with offset x by wcwidth(character) rather than 1 */

            /* Took me forever to find that function, thanks
               Andreas (newsbeuter) for that one. */

            int rwidth = wcwidth(dest[0]);
            if (rwidth > (width - *x))
                return 0;

            dest[1] = 0;
            mvwaddwstr(win, *y, *x, dest);
            *x += rwidth;
            *i += bytes;
        }
    } else
        mvwaddch(win, *y, (*x)++, str[0]);

    return 1;
}

static PyObject * mvw(PyObject *self, PyObject *args)
{
    int y, x, width, wrap;
    char *message;
    PyObject *window;
    WINDOW *win;

    if(!PyArg_ParseTuple(args, "Oiiiis", 
                &window, &y, &x, &width, &wrap, &message))
            return NULL;

    if (window != Py_None)
        win = ((PyCursesWindowObject *)window)->win;
    else
        win = NULL;
    
    /* Make width relative to current x */
    width += x;

    int i = 0;
    for (i = 0; x <= width; i++) {
        if (!message[i]) {
            return Py_BuildValue("si", NULL, x);
        } else if (message[i] == '\n') {
            i++;
            wmove(win, y, x);
            wclrtoeol(win);
            break;
        } else if (message[i] == '\\') {
            i++;
            putxy(win, width, &i, &y, &x, &message[i]);
        } else if (message[i] == '%') {
            i++;
            if (!message[i])
                return Py_BuildValue("si", NULL, x);
            else
                style_box(win, message[i]);
        } else { 
            putxy(win, width, &i, &y, &x, &message[i]);

            /* Handle intelligent wrapping on words by ensuring
               that the next word can fit, or bail on the line. */

            if ((wrap)&&(message[i] == ' ')) {
                int tmp = theme_strlen(&message[i + 1], ' ');
                if ((tmp >= (width - x)) && (tmp < width)) {
                    i++;
                    break;
                }
            }
        }
    }

    return Py_BuildValue("si", &message[i], x);
}


static PyMethodDef MvWMethods[] = {
    {"core", mvw, METH_VARARGS, "Wide char print."},
    {"tlen", tlen, METH_VARARGS, "Len ignoring theme escapes, and accounting for Unicode character width."},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initwidecurse()
{
    Py_InitModule("widecurse", MvWMethods);
}
