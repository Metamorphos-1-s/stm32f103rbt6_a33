"""Small dependency-light PNG plotting helpers for diagnostic reports."""

import math

import numpy as np
from PIL import Image, ImageDraw, ImageFont


WIDTH, HEIGHT = 1440, 768
LEFT, RIGHT, TOP, BOTTOM = 110, 45, 70, 95


def _canvas(title, xlabel, ylabel):
    image = Image.new("RGB", (WIDTH, HEIGHT), "white")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    draw.text((LEFT, 20), title, fill="black", font=font)
    draw.text((WIDTH // 2 - 60, HEIGHT - 32), xlabel, fill="black", font=font)
    draw.text((10, TOP), ylabel, fill="black", font=font)
    draw.line((LEFT, TOP, LEFT, HEIGHT - BOTTOM), fill="black", width=2)
    draw.line((LEFT, HEIGHT - BOTTOM, WIDTH - RIGHT, HEIGHT - BOTTOM),
              fill="black", width=2)
    return image, draw, font


def _bounds(values):
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]
    if len(values) == 0:
        return 0.0, 1.0
    minimum, maximum = float(np.min(values)), float(np.max(values))
    if minimum == maximum:
        margin = max(1.0, abs(minimum) * 0.05)
        return minimum - margin, maximum + margin
    margin = (maximum - minimum) * 0.05
    return minimum - margin, maximum + margin


def _transform(value, minimum, maximum, low, high):
    return low + (value - minimum) * (high - low) / (maximum - minimum)


def _grid(draw, font, xmin, xmax, ymin, ymax):
    for index in range(6):
        fraction = index / 5.0
        x = LEFT + fraction * (WIDTH - LEFT - RIGHT)
        y = HEIGHT - BOTTOM - fraction * (HEIGHT - TOP - BOTTOM)
        draw.line((x, TOP, x, HEIGHT - BOTTOM), fill="#e5e5e5")
        draw.line((LEFT, y, WIDTH - RIGHT, y), fill="#e5e5e5")
        draw.text((x - 20, HEIGHT - BOTTOM + 10),
                  "%.4g" % (xmin + fraction * (xmax - xmin)),
                  fill="black", font=font)
        draw.text((LEFT - 75, y - 6),
                  "%.4g" % (ymin + fraction * (ymax - ymin)),
                  fill="black", font=font)


def line_plot(path, title, xlabel, ylabel, x, y):
    image, draw, font = _canvas(title, xlabel, ylabel)
    x = np.asarray(x, dtype=float); y = np.asarray(y, dtype=float)
    valid = np.isfinite(x) & np.isfinite(y)
    x, y = x[valid], y[valid]
    if len(y) == 0:
        draw.text((WIDTH // 2, HEIGHT // 2), "Insufficient data", fill="black")
    else:
        xmin, xmax = _bounds(x); ymin, ymax = _bounds(y)
        _grid(draw, font, xmin, xmax, ymin, ymax)
        points = [(_transform(value_x, xmin, xmax, LEFT, WIDTH - RIGHT),
                   _transform(value_y, ymin, ymax, HEIGHT - BOTTOM, TOP))
                  for value_x, value_y in zip(x, y)]
        if len(points) == 1:
            draw.ellipse((points[0][0] - 2, points[0][1] - 2,
                          points[0][0] + 2, points[0][1] + 2), fill="#1565c0")
        else:
            draw.line(points, fill="#1565c0", width=2)
    image.save(path, "PNG")


def histogram(path, title, xlabel, ylabel, values):
    values = np.asarray(values, dtype=float)
    bins = max(10, min(80, int(math.sqrt(len(values)))))
    counts, edges = np.histogram(values, bins=bins)
    centers = (edges[:-1] + edges[1:]) / 2.0
    line_plot(path, title, xlabel, ylabel, centers, counts)


def bar_plot(path, title, ylabel, labels, values):
    image, draw, font = _canvas(title, "Experiment", ylabel)
    values = np.asarray(values, dtype=float)
    if len(values) == 0:
        draw.text((WIDTH // 2, HEIGHT // 2), "Insufficient data", fill="black")
        image.save(path, "PNG"); return
    ymin, ymax = _bounds(np.append(values, 0.0))
    _grid(draw, font, 0, max(1, len(values)), ymin, ymax)
    area = WIDTH - LEFT - RIGHT
    width = area / max(1, len(values)) * 0.65
    zero = _transform(0.0, ymin, ymax, HEIGHT - BOTTOM, TOP)
    for index, (label, value) in enumerate(zip(labels, values)):
        center = LEFT + (index + 0.5) * area / len(values)
        top = _transform(value, ymin, ymax, HEIGHT - BOTTOM, TOP)
        draw.rectangle((center - width / 2, min(zero, top),
                        center + width / 2, max(zero, top)), fill="#1565c0")
        draw.text((center - width / 2, HEIGHT - BOTTOM + 30),
                  str(label)[:18], fill="black", font=font)
    image.save(path, "PNG")
