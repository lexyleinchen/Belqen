#include "font.h"
#include "graphics.h"

static const uint8_t font_start[5] = {
    0b01110,
    0b10001,
    0b10000,
    0b10001,
    0b01110
};

static const uint8_t font_a[5] = {
    0b01110,
    0b10001,
    0b11111,
    0b10001,
    0b10001
};

static const uint8_t font_b[5] = {
    0b11110,
    0b10001,
    0b11110,
    0b10001,
    0b11110
};

static const uint8_t font_c[5] = {
    0b01111,
    0b10000,
    0b10000,
    0b10000,
    0b01111
};

static const uint8_t font_d[5] = {
    0b11110,
    0b10001,
    0b10001,
    0b10001,
    0b11110
};

static const uint8_t font_e[5] = {
    0b11111,
    0b10000,
    0b11110,
    0b10000,
    0b11111
};

static const uint8_t font_f[5] = {
    0b11111,
    0b10000,
    0b11110,
    0b10000,
    0b10000
};

static const uint8_t font_g[5] = {
    0b01111,
    0b10000,
    0b10011,
    0b10001,
    0b01110
};

static const uint8_t font_h[5] = {
    0b10001,
    0b10001,
    0b11111,
    0b10001,
    0b10001
};

static const uint8_t font_i[5] = {
    0b01110,
    0b00100,
    0b00100,
    0b00100,
    0b01110
};

static const uint8_t font_j[5] = {
    0b00111,
    0b00010,
    0b00010,
    0b10010,
    0b01100
};

static const uint8_t font_k[5] = {
    0b10001,
    0b10010,
    0b11100,
    0b10010,
    0b10001
};

static const uint8_t font_l[5] = {
    0b10000,
    0b10000,
    0b10000,
    0b10000,
    0b11111
};

static const uint8_t font_m[5] = {
    0b10001,
    0b11011,
    0b10101,
    0b10001,
    0b10001
};

static const uint8_t font_n[5] = {
    0b10001,
    0b11001,
    0b10101,
    0b10011,
    0b10001
};

static const uint8_t font_o[5] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

static const uint8_t font_p[5] = {
    0b11110,
    0b10001,
    0b11110,
    0b10000,
    0b10000
};

static const uint8_t font_q[5] = {
    0b01110,
    0b10001,
    0b10001,
    0b10011,
    0b01111
};

static const uint8_t font_r[5] = {
    0b11110,
    0b10001,
    0b11110,
    0b10010,
    0b10001
};

static const uint8_t font_s[5] = {
    0b01111,
    0b10000,
    0b01110,
    0b00001,
    0b11110
};

static const uint8_t font_t[5] = {
    0b11111,
    0b00100,
    0b00100,
    0b00100,
    0b00100
};

static const uint8_t font_u[5] = {
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

static const uint8_t font_v[5] = {
    0b10001,
    0b10001,
    0b10001,
    0b01010,
    0b00100
};

static const uint8_t font_w[5] = {
    0b10001,
    0b10001,
    0b10101,
    0b11011,
    0b10001
};

static const uint8_t font_x[5] = {
    0b10001,
    0b01010,
    0b00100,
    0b01010,
    0b10001
};

static const uint8_t font_y[5] = {
    0b10001,
    0b01010,
    0b00100,
    0b00100,
    0b00100
};

static const uint8_t font_z[5] = {
    0b11111,
    0b00010,
    0b00100,
    0b01000,
    0b11111
};

static const uint8_t font_0[5] = {
    0b11111,
    0b10011,
    0b10101,
    0b11001,
    0b11111
};

static const uint8_t font_1[5] = {
    0b00100,
    0b11100,
    0b00100,
    0b00100,
    0b11111
};

static const uint8_t font_2[5] = {
    0b01100,
    0b10010,
    0b00100,
    0b01000,
    0b11111
};

static const uint8_t font_3[5] = {
    0b11110,
    0b00001,
    0b01110,
    0b00001,
    0b11110
};

static const uint8_t font_4[5] = {
    0b00110,
    0b01010,
    0b11111,
    0b00010,
    0b00010
};

static const uint8_t font_5[5] = {
    0b11111,
    0b10000,
    0b11110,
    0b00001,
    0b11110
};

static const uint8_t font_6[5] = {
    0b01110,
    0b10000,
    0b11110,
    0b10001,
    0b01110
};

static const uint8_t font_7[5] = {
    0b11111,
    0b00001,
    0b00010,
    0b00100,
    0b01000
};

static const uint8_t font_8[5] = {
    0b01110,
    0b10001,
    0b01110,
    0b10001,
    0b01110
};

static const uint8_t font_9[5] = {
    0b01110,
    0b10001,
    0b01111,
    0b00001,
    0b01110
};

static const uint8_t font_space[5] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

static const uint8_t font_exclamationmark[5] = {
    0b00100,
    0b00100,
    0b00100,
    0b00000,
    0b00100
};

static const uint8_t font_dot[5] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b10000
};

static const uint8_t font_doublepoint[5] = {
    0b00000,
    0b10000,
    0b00000,
    0b10000,
    0b00000
};

static const uint8_t font_verticalbar[5] = {
    0b00100,
    0b00100,
    0b00100,
    0b00100,
    0b00100
};

static const uint8_t font_equals[5] = {
    0b00000,
    0b11111,
    0b00000,
    0b11111,
    0b00000
};

static const uint8_t font_underscore[5] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b11111
};

static const uint8_t font_minus[5] = {
    0b00000,
    0b00000,
    0b11111,
    0b00000,
    0b00000
};

static const uint8_t font_plus[5] = {
    0b00100,
    0b00100,
    0b11111,
    0b00100,
    0b00100
};

static const uint8_t font_percent[5] = {
    0b11001,
    0b10010,
    0b00100,
    0b01001,
    0b10011
};

static const uint8_t font_square_bracket_open[5] = {
    0b01111,
    0b01000,
    0b01000,
    0b01000,
    0b01111
};

static const uint8_t font_square_bracket_close[5] = {
    0b11110,
    0b00010,
    0b00010,
    0b00010,
    0b11110
};

static const uint8_t font_unknown[5] = {
    0b11111,
    0b10001,
    0b10101,
    0b10001,
    0b11111
};

static const uint8_t* get_character(char character) {
    if (character >= 'A' && character <= 'Z') {
        character = character - 'A' + 'a'; // Convert to lowercase
    }

    switch (character) {
        case 'a': return font_a;
        case 'b': return font_b;
        case 'c': return font_c;
        case 'd': return font_d;
        case 'e': return font_e;
        case 'f': return font_f;
        case 'g': return font_g;
        case 'h': return font_h;
        case 'i': return font_i;
        case 'j': return font_j;
        case 'k': return font_k;
        case 'l': return font_l;
        case 'm': return font_m;
        case 'n': return font_n;
        case 'o': return font_o;
        case 'p': return font_p;
        case 'q': return font_q;
        case 'r': return font_r;
        case 's': return font_s;
        case 't': return font_t;
        case 'u': return font_u;
        case 'v': return font_v;
        case 'w': return font_w;
        case 'x': return font_x;
        case 'y': return font_y;
        case 'z': return font_z;
        case '0': return font_0;
        case '1': return font_1;
        case '2': return font_2;
        case '3': return font_3;
        case '4': return font_4;
        case '5': return font_5;
        case '6': return font_6;
        case '7': return font_7;
        case '8': return font_8;
        case '9': return font_9;
        case ' ': return font_space;
        case '!': return font_exclamationmark;
        case '.': return font_dot;
        case ':': return font_doublepoint;
        case '|': return font_verticalbar;
        case '=': return font_equals;
        case '_': return font_underscore;
        case '-': return font_minus;
        case '+': return font_plus;
        case '%': return font_percent;
        case '[': return font_square_bracket_open;
        case ']': return font_square_bracket_close;
        default:  return font_unknown; // Return unknown character for unsupported characters
    }
}

void font_draw_char(int x, int y, char character, uint32_t color) {
    const uint8_t* bitmap = get_character(character);

    for (int row = 0; row < 5; row++) {
        for (int column = 0; column < 5; column++) {
            if (bitmap[row] & (1 << (4 - column))) {
                graphics_rectangle(x + column * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}

void font_draw_text(int x, int y, const char* text, uint32_t color) {
    int current_x = x;

    while(*text != '\0') {
        font_draw_char(current_x, y, *text, color);
        current_x += 14; // Move to the next character position (5 pixels + 2 pixels spacing)
        text++;
    }
}

void font_draw_number(int x, int y, uint32_t number, uint32_t color) {
    char buffer[16];
    int position = 0;

    if (number == 0) {
        buffer[position++] = '0';
    }
    else {
        char reversed[16];
        int count = 0;

        while (number > 0 && count < 15) {
            reversed[count++] = '0' + (number % 10);
            number /= 10;
        }

        while (count > 0) {
            buffer[position++] = reversed[--count];
        }
    }

    buffer[position] = '\0';
    font_draw_text(x, y, buffer, color);
}