"""Write ui_arrows.c: antialiased diagonal chevrons as LVGL 8-bit alpha images.

The straight jog buttons use the font's chevrons (LV_SYMBOL_UP etc.). LVGL 6
has no diagonal ones and draws thick diagonal lines poorly, so the diagonal
chevrons are pre-rendered here: the up chevron turned 45 degrees, i.e. a
corner with one arm along each axis, the same stroke as a 16 px glyph.
"""
import math
import sys

SIZE = 18
WIDTH = 3.0         # stroke, px
SS = 6              # supersampling per axis

# Up-right: tip in the top right corner, arms to the left and down
TIP = (13.0, 4.5)
ARMS = [(3.5, 4.5), (13.0, 14.0)]


def dist_seg(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    cx, cy = ax + t * dx, ay + t * dy
    return math.hypot(px - cx, py - cy)


def render():
    img = [[0] * SIZE for _ in range(SIZE)]
    for y in range(SIZE):
        for x in range(SIZE):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    px = x + (sx + 0.5) / SS
                    py = y + (sy + 0.5) / SS
                    d = min(dist_seg(px, py, TIP[0] + 0.5, TIP[1] + 0.5, a[0] + 0.5, a[1] + 0.5) for a in ARMS)
                    if d <= WIDTH / 2:
                        hits += 1
            img[y][x] = round(255 * hits / (SS * SS))
    return img


def flip(img, fx, fy):
    return [[img[SIZE - 1 - y if fy else y][SIZE - 1 - x if fx else x] for x in range(SIZE)] for y in range(SIZE)]


def c_array(name, img):
    out = ['static const uint8_t %s_map[] = {' % name]
    for row in img:
        out.append('    ' + ', '.join('0x%02x' % v for v in row) + ',')
    out.append('};')
    out.append('')
    out.append('const lv_img_dsc_t %s = {' % name)
    out.append('    .header = { .cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, .w = %d, .h = %d },' % (SIZE, SIZE))
    out.append('    .data_size = sizeof(%s_map),' % name)
    out.append('    .data = %s_map' % name)
    out.append('};')
    return '\n'.join(out)


base = render()
parts = [
    ('ui_arrow_up_right', base),
    ('ui_arrow_up_left', flip(base, True, False)),
    ('ui_arrow_down_right', flip(base, False, True)),
    ('ui_arrow_down_left', flip(base, True, True)),
]

header = '''/*
 * ui_arrows.c - Diagonal jog chevrons, %dx%d 8-bit alpha images
 *
 * Part of grblHAL TFT Plugin
 *
 * Generated: the up chevron turned 45 degrees, antialiased. LVGL 6 has no
 * diagonal chevron glyph and draws thick diagonal lines with jagged edges,
 * so these are images, colored like text by the button style (image.color).
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <lvgl.h>

#include "ui_arrows.h"
''' % (SIZE, SIZE)

body = '\n\n'.join(c_array(n, i) for n, i in parts)
open(sys.argv[1], 'w', newline='\n').write(header + '\n' + body + '\n\n#endif // TFT_ENABLE\n')

# Preview
for row in base:
    print(''.join(' .:-=+*#%@'[min(9, v * 10 // 256)] for v in row))
