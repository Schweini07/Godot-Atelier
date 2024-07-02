
#include "drawing_algorithms.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;


void DrawingAlgorithms::_bind_methods() {
    //ClassDB::bind_method(D_METHOD("get_ellipse_points", "pos", "size"), &DrawingAlgorithms::get_ellipse_points);
    ClassDB::bind_static_method("DrawingAlgorithms", D_METHOD("get_ellipse_points", "pos", "size"), &DrawingAlgorithms::get_ellipse_points);
    ClassDB::bind_static_method("DrawingAlgorithms", D_METHOD("get_ellipse_points_filled", "pos", "size", "thickness"), &DrawingAlgorithms::get_ellipse_points_filled, DEFVAL(1));
}

TypedArray<Vector2i> DrawingAlgorithms::get_ellipse_points(Vector2i pos, Vector2i size) {
    TypedArray<Vector2i> array;
	int x0 = pos.x;
	int x1 = pos.x + (size.x - 1);
	int y0 = pos.y;
	int y1 = pos.y + (size.y - 1);
	int a = godot::Math::abs(x1 - x0);
	int b = godot::Math::abs(y1 - x0);
	int b1 = b & 1;
	int dx = 4 * (1 - a) * b * b;
	int dy = 4 * (b1 + 1) * a * a;
	int err = dx + dy + b1 * a * a;
	int e2 = 0;

	if (x0 > x1) {
		x0 = x1;
		x1 += a;
    }

	if (y0 > y1)
		y0 = y1;

	y0 += (b + 1) / 2;
	y1 = y0 - b1;
	a *= 8 * a;
	b1 = 8 * b * b;

	while (x0 <= x1) {
		Vector2i v1 = Vector2i(x1, y0);
		Vector2i v2 = Vector2i(x0, y0);
		Vector2i v3 = Vector2i(x0, y1);
		Vector2i v4 = Vector2i(x1, y1);
		array.append(v1);
		array.append(v2);
		array.append(v3);
		array.append(v4);

		e2 = 2 * err;
		if (e2 <= dy) {
			y0 += 1;
			y1 -= 1;
			dy += a;
			err += dy;
        }

		if (e2 >= dx || 2 * err > dy) {
			x0 += 1;
			x1 -= 1;
			dx += b1;
			err += dx;
        }
    }

	while (y0 - y1 < b) {
		Vector2i v1 = Vector2i(x0 - 1, y0);
		Vector2i v2 = Vector2i(x1 + 1, y0);
		Vector2i v3 = Vector2i(x0 - 1, y1);
		Vector2i v4 = Vector2i(x1 + 1, y1);
		array.append(v1);
		array.append(v2);
		array.append(v3);
		array.append(v4);
		y0 += 1;
		y1 -= 1;
    }

	return array;
}

TypedArray<Vector2i> DrawingAlgorithms::get_ellipse_points_filled(Vector2i pos, Vector2i size, int thickness) {
    Vector2i offsetted_size = size + Vector2i(1, 1) * (thickness - 1);
	TypedArray<Vector2i> border = get_ellipse_points(pos, offsetted_size);
	TypedArray<Vector2i> filling;

	for (int x = 1; x < godot::Math::ceil(offsetted_size.x / 2.0); x++) {
		bool fill = false;
		bool prev_is_true = false;
		for (int y = 0; y < godot::Math::ceil(offsetted_size.y / 2.0); y++) {
			Vector2i top_l_p = Vector2i(x, y);
			bool bit = border.has(pos + top_l_p);

			if (bit && !fill) {
				prev_is_true = true;
				continue;
            }

			if (!bit && (fill || prev_is_true)) {
				filling.append(pos + top_l_p);
				filling.append(pos + Vector2i(x, offsetted_size.y - y - 1));
				filling.append(pos + Vector2i(offsetted_size.x - x - 1, y));
				filling.append(pos + Vector2i(offsetted_size.x - x - 1, offsetted_size.y - y - 1));

				if (prev_is_true) {
					fill = true;
					prev_is_true = false;
                }
            }
			else if (bit && fill)
				break;
        }
    }

	return border + filling;
}

DrawingAlgorithms::DrawingAlgorithms() {}

DrawingAlgorithms::~DrawingAlgorithms() {}
