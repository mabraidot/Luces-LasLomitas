# -*- coding: utf-8 -*-
"""
Genera el icono de la app: un foco encendido sobre el fondo de la web.

Se dibuja con distancias analiticas y antialiasing de 1 px, sin dependencias
externas (solo zlib de la biblioteca estandar). Las formas son las mismas del
SVG de la pagina, escaladas y centradas dentro de la zona segura de un icono
'maskable' de Android (circulo central del 80%).
"""

import io
import math
import struct
import zlib

SIZE = 512

# Paleta, la misma de la pagina
BG    = (0x0d, 0x11, 0x17)
AMBER = (0xff, 0xb8, 0x4d)
CORE  = (0xff, 0xf6, 0xe0)
CAP   = (0x46, 0x4f, 0x5b)
GLOW  = (0xff, 0xa0, 0x28)

# Caja del dibujo REAL, no del viewBox del SVG: el vidrio arranca en y=14
# (cy 50 menos r 36) y la punta del casquillo termina en y=135. El viewBox
# llega a 176, asi que centrar sobre el viewBox deja el foco corrido arriba.
CONTENT_X0, CONTENT_X1 = 24.0, 96.0
CONTENT_Y0, CONTENT_Y1 = 14.0, 135.0
CONTENT_CX = (CONTENT_X0 + CONTENT_X1) / 2.0
CONTENT_CY = (CONTENT_Y0 + CONTENT_Y1) / 2.0

# Alto en pixeles del dibujo. La media diagonal de la caja queda en ~198 px,
# por debajo del radio seguro de 205 px de un icono maskable de 512.
TARGET_H = 340.0
SCALE = TARGET_H / (CONTENT_Y1 - CONTENT_Y0)


def sx(v):
    return SIZE / 2.0 + (v - CONTENT_CX) * SCALE


def sy(v):
    return SIZE / 2.0 + (v - CONTENT_CY) * SCALE


# --- formas, en coordenadas de icono ---------------------------------------
GLASS_CX, GLASS_CY, GLASS_R = sx(60), sy(50), 36 * SCALE
CORE_R = GLASS_R * 0.46
GLOW_RANGE = GLASS_R * 1.45

NECK = (sy(78), sy(96), (sx(40), sx(80)), (sx(44), sx(76)))
CAP1 = (sx(43), sx(77), sy(98), sy(107))
CAP2 = (sx(43), sx(77), sy(110), sy(119))
TIP  = (sy(122), sy(135), (sx(47), sx(73)), (sx(50), sx(70)))


def clamp01(v):
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def cover_span(v, lo, hi):
    """Cobertura de un pixel dentro del intervalo [lo, hi], con AA de 1 px."""
    return clamp01(min(v - lo + 0.5, hi - v + 0.5))


def cover_rect(x, y, x0, x1, y0, y1):
    return cover_span(x, x0, x1) * cover_span(y, y0, y1)


def cover_trapezoid(x, y, shape):
    y0, y1, top, bottom = shape
    if y < y0 - 1 or y > y1 + 1:
        return 0.0
    t = clamp01((y - y0) / (y1 - y0))
    x0 = top[0] + (bottom[0] - top[0]) * t
    x1 = top[1] + (bottom[1] - top[1]) * t
    return cover_span(x, x0, x1) * cover_span(y, y0, y1)


def blend(dst, src, alpha):
    if alpha <= 0.0:
        return dst
    if alpha >= 1.0:
        return (float(src[0]), float(src[1]), float(src[2]))
    inv = 1.0 - alpha
    return (dst[0] * inv + src[0] * alpha,
            dst[1] * inv + src[1] * alpha,
            dst[2] * inv + src[2] * alpha)


def render():
    rows = []
    for py in range(SIZE):
        y = py + 0.5
        row = bytearray()
        for px in range(SIZE):
            x = px + 0.5
            c = (float(BG[0]), float(BG[1]), float(BG[2]))

            # Halo: cae hacia afuera del vidrio, cuadraticamente.
            d = math.hypot(x - GLASS_CX, y - GLASS_CY)
            if d > GLASS_R:
                t = clamp01(1.0 - (d - GLASS_R) / GLOW_RANGE)
                c = blend(c, GLOW, 0.60 * t * t)

            # Cuello y vidrio (el orden es el del SVG)
            c = blend(c, AMBER, cover_trapezoid(x, y, NECK))
            c = blend(c, AMBER, clamp01(0.5 - (d - GLASS_R)))

            # Nucleo brillante: sugiere el filamento sin dibujarlo, que a
            # tamano de icono se volveria un borron.
            if d < CORE_R * 2.0:
                t = clamp01(1.0 - (d / CORE_R))
                c = blend(c, CORE, 0.85 * t * t)

            # Casquillo, por encima del cuello
            c = blend(c, CAP, cover_rect(x, y, CAP1[0], CAP1[1], CAP1[2], CAP1[3]))
            c = blend(c, CAP, cover_rect(x, y, CAP2[0], CAP2[1], CAP2[2], CAP2[3]))
            c = blend(c, CAP, cover_trapezoid(x, y, TIP))

            row.append(int(clamp01(c[0] / 255.0) * 255 + 0.5))
            row.append(int(clamp01(c[1] / 255.0) * 255 + 0.5))
            row.append(int(clamp01(c[2] / 255.0) * 255 + 0.5))
        rows.append(bytes(row))
    return rows


def chunk(tag, data):
    return (struct.pack('>I', len(data)) + tag + data
            + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))


def quantize(rows):
    """Reduce a <=256 colores redondeando cada canal a una rejilla."""
    for step in (1, 2, 3, 4, 6, 8):
        table = {}
        order = []
        out = []
        overflow = False
        for row in rows:
            line = bytearray()
            for i in range(0, len(row), 3):
                key = (row[i] // step * step,
                       row[i + 1] // step * step,
                       row[i + 2] // step * step)
                idx = table.get(key)
                if idx is None:
                    if len(order) >= 256:
                        overflow = True
                        break
                    idx = len(order)
                    table[key] = idx
                    order.append(key)
                line.append(idx)
            if overflow:
                break
            out.append(bytes(line))
        if not overflow:
            print('paleta: %d colores (paso %d)' % (len(order), step))
            return out, order
    raise RuntimeError('no se pudo reducir a 256 colores')


def to_png(rows):
    indexed, palette = quantize(rows)

    raw = bytearray()
    for row in indexed:
        # Filtro 0 (None): sobre indices de paleta, restar filas mezclaria
        # colores que no guardan relacion numerica y comprimiria peor.
        raw.append(0)
        raw.extend(row)

    plte = bytearray()
    for r, g, b in palette:
        plte.extend((r, g, b))

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 3, 0, 0, 0))
    out += chunk(b'PLTE', bytes(plte))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += chunk(b'IEND', b'')
    return out


png = to_png(render())
print('PNG: %d bytes' % len(png))

with io.open('icon.png', 'wb') as f:
    f.write(png)

# --- header C -------------------------------------------------------------
lines = []
for i in range(0, len(png), 16):
    part = ', '.join('0x%02x' % b for b in png[i:i + 16])
    lines.append('  ' + part + ',')

header = u'''/*
  Icon.h - Icono de la app, en PROGMEM.

  PNG de %d x %d, generado con el mismo dibujo del foco de la pagina y el
  mismo color de fondo. Android no usa el SVG de la pagina para el acceso
  directo de la pantalla de inicio: necesita un mapa de bits.

  El foco esta centrado dentro del circulo central del 80%%, que es la zona
  segura de un icono 'maskable': asi Android puede recortarlo en circulo o en
  cuadrado redondeado sin cortar el dibujo, y rellena el resto con este mismo
  fondo en lugar de poner un cuadrado blanco.

  Se sirve desde /i.png. NO editar a mano: regenerar con scratchpad/icon.py.
*/

#ifndef ICON_H
#define ICON_H

#include <Arduino.h>

#define ICON_PNG_LEN %d

static const uint8_t ICON_PNG[] PROGMEM = {
%s
};

#endif // ICON_H
''' % (SIZE, SIZE, len(png), '\n'.join(lines))

with io.open('Icon.h', 'w', encoding='utf-8') as f:
    f.write(header)

print('Icon.h listo')
