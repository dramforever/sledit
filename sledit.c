#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Terminal IO

#define BS  8
#define LF  10
#define CR  13
#define ESC 27
#define DEL 127

static inline size_t ctrl(char x) {
    return x & ~0x40;
}

enum Key {
    K_UP = 256,
    K_DOWN,
    K_RIGHT,
    K_LEFT,
    K_DELETE,
    K_HOME,
    K_END,
};

uint8_t rawkey() {
    uint8_t byte;
    ssize_t ret = read(STDIN_FILENO, &byte, 1);
    assert(ret == 1);
    return byte;
}

void emit(uint8_t byte) {
    ssize_t ret = write(STDOUT_FILENO, &byte, 1);
    assert(ret == 1);
}

size_t key() {
    size_t k = rawkey();
    if (k != ESC) return k;

    size_t k1 = rawkey();
    if (k1 == '[') {
        size_t k2 = rawkey();
        if (k2 == 'A') return K_UP;
        else if (k2 == 'B') return K_DOWN;
        else if (k2 == 'C') return K_RIGHT;
        else if (k2 == 'D') return K_LEFT;
        else if (k2 == '1' || k2 == '3' || k2 == '4') {
            size_t k3 = rawkey();
            if (k3 == '~') {
                if (k2 == '1') return K_HOME;
                else if (k2 == '4') return K_END;
                else if (k2 == '3') return K_DELETE;
                else return 0;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

// Editor state

#define BUFFER_SIZE 4096

char buffer[BUFFER_SIZE];
size_t startgap = 0;
size_t endgap = BUFFER_SIZE;
size_t line = 0;
size_t cursor = 0;
size_t lineno = 1;

void insert(char c) {
    if (startgap == endgap) return;

    for (size_t i = startgap; i != cursor; i--)
        buffer[i] = buffer[i - 1];
    buffer[cursor] = c;
    startgap++;
}

void erase() {
    if (cursor == startgap) return;

    for (size_t i = cursor; i != startgap; i++)
        buffer[i] = buffer[i + 1];

    startgap--;
}

void moveback(size_t newsg) {
    size_t len = startgap - newsg;
    startgap = newsg;
    endgap -= len;
    for (size_t i = len - 1; i != (size_t) -1; i--)
        buffer[endgap + i] = buffer[startgap + i];
}

void prevline() {
    if (line == 0) return;

    size_t prev = line - 1;
    while (prev > 0 && buffer[prev - 1] != LF)
        prev--;

    moveback(line - 1);

    line = prev;
    lineno--;
}

void moveforward(size_t neweg) {
    size_t len = neweg - endgap;

    for (size_t i = 0; i != len; i++)
        buffer[startgap + i] = buffer[endgap + i];

    endgap = neweg;
    startgap += len;
}

void nextline() {
    if (endgap == BUFFER_SIZE) return;

    size_t next = endgap + 1;
    while (next < BUFFER_SIZE && buffer[next] != LF)
        next++;

    size_t saved = startgap;
    moveforward(next);
    line = saved + 1;
    lineno++;
}

// Decimal output

size_t num(char *buf, size_t n) {
    size_t n1 = n;
    do {
        buf++;
    } while (n /= 10);
    char *bend = buf;
    do {
        *(--buf) = '0' + n1 % 10;
    } while (n1 /= 10);
    return bend - buf;
}

void emitnum(size_t n) {
    char buf[32];
    size_t len = num(buf, n);
    for (size_t i = 0; i != len; i++) {
        emit(buf[i]);
    }
}

size_t emitr(size_t n, size_t width) {
    char buf[32];
    size_t len = num(buf, n);
    for (size_t i = len; i < width; i++)
        emit(' ');
    for (size_t i = 0; i < len; i++)
        emit(buf[i]);
    return len < width ? width : len;
}

// Complex terminal escape sequences

void sgr(size_t num) {
    emit(ESC);
    emit('[');
    emitnum(num);
    emit('m');
}

void linedone() {
    emit(ESC);
    emit('[');
    emit('K');
}

void pos(size_t pos) {
    emit(ESC);
    emit('[');
    emitnum(pos);
    emit('G');
}

void bottom() {
    emit(LF);
    emit(ESC);
    emit('[');
    emitnum(100000);
    emit('H');
}

void draw() {
    emit(CR);
    sgr(4);
    size_t pre = emitr(lineno, 6);
    sgr(0);
    emit(' ');

    for (size_t i = line; i != startgap; i++)
        emit(buffer[i]);

    linedone();
    pos(cursor - line + pre + 2);
}

// Listing the current buffer

void dump() {
    emit(CR);
    linedone();

    size_t saved = startgap;
    moveforward(BUFFER_SIZE);

    size_t start = 0;
    size_t n = 1;

    for (size_t i = 0; i <= startgap; i++) {
        if (i == startgap || buffer[i] == LF) {
            if (i == saved) sgr(4);

            emitr(n, 6);
            emit(' ');
            for (size_t j = start; j != i; j++)
                emit(buffer[j]);

            emit(CR);
            emit(LF);

            if (i == saved) sgr(0);
            start = i + 1;
            n++;
        }
    }

    moveback(saved);

    emit(CR);
    emit(LF);
}

// Editor mainloop

void edit() {
    bottom();
    for (;;) {
        draw();

        size_t k = key();
        if (k == ctrl('C')) {
            break;
        } else if (k == ctrl('L')) {
            dump();
        } else if (k == BS || k == DEL) {
            if (cursor > line) {
                cursor--;
                erase();
            } else if (line > 0) {
                if (line != cursor) exit(1);
                cursor--;
                erase();
                do
                    line--;
                while (line > 0 && buffer[line - 1] != LF);
                lineno--;
            }
        } else if (k == K_DELETE) {
            if (cursor < startgap) {
                erase();
            } else if (endgap != BUFFER_SIZE) {
                endgap++;
                size_t eg = endgap;
                while (eg != BUFFER_SIZE && buffer[eg] != LF)
                    eg++;
                moveforward(eg);
            }
        } else if (k == CR) {
            insert(LF);
            cursor++;
            line = cursor;
            lineno++;
        } else if (k == K_HOME) {
            cursor = line;
        } else if (k == K_END) {
            cursor = startgap;
        } else if (k == K_LEFT) {
            if (cursor > 0) {
                cursor--;
                if (cursor < line) prevline();
            }
        } else if (k == K_RIGHT) {
            if (cursor < startgap || endgap != BUFFER_SIZE) {
                cursor++;
                if (cursor > startgap) nextline();
            }
        } else if (k == K_UP) {
            size_t col = cursor - line;
            prevline();
            if (col > startgap - line) cursor = startgap;
            else cursor = line + col;
        } else if (k == K_DOWN) {
            size_t col = cursor - line;
            nextline();
            if (col > startgap - line) cursor = startgap;
            else cursor = line + col;
        } else if (32 <= k && k < 127) {
            insert(k);
            cursor++;
        }
    }
}

int main() {
    edit();
}
