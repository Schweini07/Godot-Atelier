#ifndef DRAWING_ALGORITHMS_H
#define DRAWING_ALGORITHMS_H

#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {
    class DrawingAlgorithms : public RefCounted {
        GDCLASS(DrawingAlgorithms, RefCounted)

        protected:
	        static void _bind_methods();
        public:
            unsigned int iterations = 100000;
            static TypedArray<Vector2i> get_ellipse_points(Vector2i pos, Vector2i size);
            static TypedArray<Vector2i> get_ellipse_points_filled(Vector2i pos, Vector2i size, int thickness);
            DrawingAlgorithms();
            ~DrawingAlgorithms();
    };
}
#endif
