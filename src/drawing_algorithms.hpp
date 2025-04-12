#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class Image;
class Shader;

class DrawingAlgosCpp : public RefCounted
{
    GDCLASS(DrawingAlgosCpp, RefCounted)

protected:
    static void _bind_methods();

private:
    enum RotationAlgorithm { ROTXEL_SMEAR, CLEANEDGE, OMNISCALE, NNS, NN, ROTXEL, URD };
    enum Interpolation { SCALE3X_INTERPOLATION = 5, CLEANEDGE_INTERPOLATION = 6, OMNISCALE_INTERPOLATION = 7 };

    Ref<Shader> rotxel_shader;
    Ref<Shader> clean_edge_shader;
    Ref<Shader> omniscale_shader;
    Ref<Shader> nn_shader;

public:
    DrawingAlgosCpp();
    ~DrawingAlgosCpp();

    static void SetLayerMetadataImage(Ref<RefCounted> layer, Ref<RefCounted> cel, Ref<Image>, int index, bool include = true); // TODO: Broken
    static void BlendLayersHeadless(Ref<Image> image, Ref<RefCounted> project, Ref<RefCounted> layer, Ref<RefCounted> cel, Vector2i origin);
    static TypedArray<Vector2i> GetEllipsePoints(Vector2i pos, Vector2i size);
    static TypedArray<Vector2i> GetEllipsePointsFilled(Vector2i pos, Vector2i size, int thickness);
    static Ref<Image> Scale3x(Ref<Image> sprite, float tol = 0.196078);
    void Transform(Ref<Image> image, Dictionary params, RotationAlgorithm algorithm, bool expand = false); // TODO: Function should be static, but can't because of non-static members
    static bool TypeIsShader(RotationAlgorithm algorithm);
    static Rect2 TransformRectangle(Rect2 rect, Transform2D matrix, Vector2 pivot);
    static void Rotxel(Ref<Image> sprite, float angle, Vector2 pivot);
    static void FakeRotsprite(Ref<Image> sprite, float angle, Vector2 pivot);
    static void NNRotate(Ref<Image> sprite, float angle, Vector2 pivot);
    static bool SimilarColors(Color c1, Color c2, float tol = 0.392157);
    static void Center(Array indices);
    void ScaleProject(int width, int height, int interpolation); // TODO: Function should be static, this problematic though because of the use of non-static variables in ResizeImage
    Ref<Image> ResizeImage(Ref<Image> image, int width, int height, int interpolation);
    static void CropToSelection();
    static void CropToContent();
    static void GeneralDoAndUndoScale(int width, int height, Dictionary redo_data, Dictionary undo_data);
};

}
