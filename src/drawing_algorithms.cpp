#include "drawing_algorithms.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/print_string.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/line2d.hpp>
#include <godot_cpp/classes/undo_redo.hpp>

using namespace godot;

void DrawingAlgosCpp::_bind_methods() {
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("set_layer_metadata_image", "layer", "cel", "image", "index", "include"), &DrawingAlgosCpp::SetLayerMetadataImage, DEFVAL(true));
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("blend_layers_headless", "image", "project", "layer", "cel", "origin"), &DrawingAlgosCpp::BlendLayersHeadless);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("get_ellipse_points", "pos", "size"), &DrawingAlgosCpp::GetEllipsePoints);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("get_ellipse_points_filled", "pos", "size", "thickness"), &DrawingAlgosCpp::GetEllipsePointsFilled, DEFVAL(1));
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("scale_3x", "sprite", "tol"), &DrawingAlgosCpp::Scale3x, DEFVAL(0.196078));
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("transform_rectangle", "rect", "matrix", "pivot"), &DrawingAlgosCpp::TransformRectangle);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("rotxel", "sprite", "angle", "pivot"), &DrawingAlgosCpp::Rotxel);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("fake_rotsprite", "sprite", "angle", "pivot"), &DrawingAlgosCpp::FakeRotsprite);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("nn_rotate", "sprite", "angle", "pivot"), &DrawingAlgosCpp::NNRotate);
    ClassDB::bind_static_method("DrawingAlgosCpp", D_METHOD("similar_colors", "c1", "c2", "tol"), &DrawingAlgosCpp::SimilarColors, DEFVAL(0.392157));
}

DrawingAlgosCpp::DrawingAlgosCpp()
{
	ResourceLoader *resource_loader = ResourceLoader::get_singleton();

	rotxel_shader = static_cast<Ref<Shader>>(resource_loader->load("res://src/Shaders/Effects/Rotation/SmearRotxel.gdshader"));
	clean_edge_shader = static_cast<Ref<Shader>>(resource_loader->load("res://src/Shaders/Effects/Rotation/cleanEdge.gdshader"));
	omniscale_shader = static_cast<Ref<Shader>>(resource_loader->load("res://src/Shaders/Effects/Rotation/OmniScale.gdshader"));
	rotxel_shader = static_cast<Ref<Shader>>(resource_loader->load("res://src/Shaders/Effects/Rotation/SmearRotxel.gdshader"));
	nn_shader = static_cast<Ref<Shader>>(resource_loader->load("res://src/Shaders/Effects/Rotation/NearestNeighbour.gdshader"));
}

DrawingAlgosCpp::~DrawingAlgosCpp()
{
}

void BlendLayers(Ref<Image> image, Ref<RefCounted> frame, Vector2i origin, Ref<RefCounted> project, bool only_selected_cels, bool only_selected_layers)
{
	frame->set_script("res://src/Classes/Frame.gd");
	project->set_script("res://src/Classes/Project.gd");

	int frame_index = project->call("frames").call("find", frame);

	Array previous_ordered_layers = project->get("ordered_layers");
	project->call("order_layers", frame_index);

	Array textures;

	int layer_count = project->get("layers").call("size");
	Ref<Image> metadata_image;
	metadata_image.instantiate();
	metadata_image->create(layer_count, 4, false, Image::FORMAT_R8);

	Array ordered_layers = project->get("ordered_layers");

	for (int i = 0; i < layer_count; i++)
	{
		int ordered_index = ordered_layers[i];
		Variant layer_variant = project->get("layers")[ordered_index];
		bool include = layer->call("is_visible_in_hierarchy");

		if (only_selected_cels && include)
		{
			Array selected_cels = project->get("selected_cels");
			Array test_array;
			test_array.append(frame_index);
			test_array.append(i);
			if (!selected_cels.has(test_array))
				include = false;
		}

		if (only_selected_layers && include) {
			Array selected_cels = project->get("selected_cels");
			bool layer_is_selected = false;
			for (int j = 0; j < selected_cels.size(); j++) {
				Array sel_cel = selected_cels[j];
				if (sel_cel[1].operator int() == i) {
					layer_is_selected = true;
					break;
				}
			}
			if (!layer_is_selected) {
				include = false;
			}
		}

		Array frame_cels = frame->get("cels");
		Ref<RefCounted> cel = frame_cels[ordered_index];

		if (DisplayServer::get_singleton()->get_name() == "headless") {
			project->call("blend_layers_headless", image, project, layer, cel, origin);
		} else {
			if (layer->call("is_blender")) {
				Ref<Image> cel_image = layer->call("blend_children", frame);
				textures.append(cel_image);
			} else {
				Ref<Image> cel_image = layer->call("display_effects", cel);
				textures.append(cel_image);
			}

			if (layer->call("is_blended_by_ancestor") && !only_selected_cels && !only_selected_layers) {
				include = false;
			}

			project->call("set_layer_metadata_image", layer, cel, metadata_image, ordered_index, include);
		}
	}

	if (DisplayServer::get_singleton()->get_name() != "headless") {
		Ref<Texture2DArray> texture_array;
		texture_array.instantiate();
		texture_array->call("create_from_images", textures);

		Dictionary params;
		params["layers"] = texture_array;
		params["metadata"] = ImageTexture::create_from_image(metadata_image);

		Vector2i size = project->get("size");
		Ref<Image> blended;
		blended.instantiate();
		blended->create(size.x, size.y, false, image->get_format());

		Ref<RefCounted> gen;
		gen.instantiate("ShaderImageEffect"); // assuming ShaderImageEffect is registered
		gen->call("generate_image", blended, project->get("blend_layers_shader"), params, size);

		image->blend_rect(blended, Rect2i(Vector2i(), size), origin);
	}

	project->set("ordered_layers", previous_ordered_layers);
}

	var frame_index := project.frames.find(frame)
	var previous_ordered_layers: Array[int] = project.ordered_layers
	project.order_layers(frame_index)
	var textures: Array[Image] = []
	var metadata_image := Image.create(project.layers.size(), 4, false, Image.FORMAT_R8)
	for i in project.layers.size():
		var ordered_index := project.ordered_layers[i]
		var layer := project.layers[ordered_index]
		var include := true if layer.is_visible_in_hierarchy() else false
		if only_selected_cels and include:
			var test_array := [frame_index, i]
			if not test_array in project.selected_cels:
				include = false
		if only_selected_layers and include:
			var layer_is_selected := false
			for selected_cel in project.selected_cels:
				if i == selected_cel[1]:
					layer_is_selected = true
					break
			if not layer_is_selected:
				include = false
		var cel := frame.cels[ordered_index]
		if DisplayServer.get_name() == "headless":
			blend_layers_headless(image, project, layer, cel, origin)
		else:
			if layer.is_blender():
				var cel_image := (layer as GroupLayer).blend_children(frame)
				textures.append(cel_image)
			else:
				var cel_image := layer.display_effects(cel)
				textures.append(cel_image)
			if (
				layer.is_blended_by_ancestor()
				and not only_selected_cels
				and not only_selected_layers
			):
				include = false
			set_layer_metadata_image(layer, cel, metadata_image, ordered_index, include)
	if DisplayServer.get_name() != "headless":
		var texture_array := Texture2DArray.new()
		texture_array.create_from_images(textures)
		var params := {
			"layers": texture_array,
			"metadata": ImageTexture.create_from_image(metadata_image),
		}
		var blended := Image.create(project.size.x, project.size.y, false, image.get_format())
		var gen := ShaderImageEffect.new()
		gen.generate_image(blended, blend_layers_shader, params, project.size)
		image.blend_rect(blended, Rect2i(Vector2i.ZERO, project.size), origin)
	# Re-order the layers again to ensure correct canvas drawing
	project.ordered_layers = previous_ordered_layers

void DrawingAlgosCpp::SetLayerMetadataImage(Ref<RefCounted> layer, Ref<RefCounted> cel, Ref<Image> image, int index, bool include)
{
	layer->set_script("res://src/Classes/Layers/BaseLayer.gd");

	// Store the blend mode
	image->set_pixel(index, 0, Color(static_cast<int>(layer->get("blend_mode")) / 255.0, 0.0, 0.0, 0.0));
	
	// Store the opacity
	if (layer->call("is_visible_in_hierarchy") && include)
	{
		float opacity = cel->call("get_final_opacity", "layer");
		image->set_pixel(index, 1, Color(opacity, 0.0, 0.0, 0.0));
	}
	else
		image->set_pixel(index, 1, Color());
	
	// Store the clipping mask boolean
	if (layer->get("clipping_mask"))
		image->set_pixel(index, 3, Color::hex(0xFF0000FF)); // RED
	else
		image->set_pixel(index, 3, Color::hex(0x000000FF)); // BLACK
	
	if (!include)
	{
		// Store a small red value as a way to indicate that this layer should be skipped
		// Used for layers such as child layers of a group, so that the group layer itself can
		// successfully be used as a clipping mask with the layer below it.
		image->set_pixel(index, 3, Color(0.2, 0.0, 0.0, 0.0));
	}
}

void DrawingAlgosCpp::BlendLayersHeadless(Ref<Image> image, Ref<RefCounted> project, Ref<RefCounted> layer, Ref<RefCounted> cel, Vector2i origin)
{
	project->set_script("res://src/Classes/Project.gd");
	layer->set_script("res://src/Classes/Layers/BaseLayer.gd");
	cel->set_script("res://src/Classes/Cels/BaseCel.gd");

	float opacity = cel->call("get_final_opacity", layer);
	Ref<Image> cel_image;
	cel_image->copy_from(cel->call("get_image"));

	if (opacity < 1.f) // If we havge cel or layer transparency
	{
		for (int x = 0; x < cel_image->get_size().x; x++)
		{
			for (int y = 0; y < cel_image->get_size().y; y++)
			{
				Color pixel_color = cel_image->get_pixel(x, y);
				pixel_color.a *= opacity;
				cel_image->set_pixel(x, y, pixel_color);
			}
		}
	}

	image->blend_rect(cel_image, Rect2i(Vector2i(0, 0), project->get("size")), origin);
}

TypedArray<Vector2i> DrawingAlgosCpp::GetEllipsePoints(Vector2i pos, Vector2i size)
{
    TypedArray<Vector2i> array;
	int x0 = pos.x;
	int x1 = pos.x + (size.x - 1);
	int y0 = pos.y;
	int y1 = pos.y + (size.y - 1);
	int a = Math::abs(x1 - x0);
	int b = Math::abs(y1 - x0);
	int b1 = b & 1;
	int dx = 4 * (1 - a) * b * b;
	int dy = 4 * (b1 + 1) * a * a;
	int err = dx + dy + b1 * a * a;
	int e2 = 0;

	if (x0 > x1)
	{
		x0 = x1;
		x1 += a;
    }

	if (y0 > y1)
		y0 = y1;

	y0 += (b + 1) / 2;
	y1 = y0 - b1;
	a *= 8 * a;
	b1 = 8 * b * b;

	while (x0 <= x1)
	{
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

	while (y0 - y1 < b)
	{
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

TypedArray<Vector2i> DrawingAlgosCpp::GetEllipsePointsFilled(Vector2i pos, Vector2i size, int thickness)
{
    Vector2i offsetted_size = size + Vector2i(1, 1) * (thickness - 1);
	TypedArray<Vector2i> border = GetEllipsePoints(pos, offsetted_size);
	TypedArray<Vector2i> filling;

	for (int x = 1; x < godot::Math::ceil(offsetted_size.x / 2.0); x++) 
	{
		bool fill = false;
		bool prev_is_true = false;
		for (int y = 0; y < godot::Math::ceil(offsetted_size.y / 2.0); y++)
		{
			Vector2i top_l_p = Vector2i(x, y);
			bool bit = border.has(pos + top_l_p);

			if (bit && !fill)
			{
				prev_is_true = true;
				continue;
            }

			if (!bit && (fill || prev_is_true))
			{
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

Ref<Image> DrawingAlgosCpp::Scale3x(Ref<Image> sprite, float tol)
{
	Ref<Image> scaled = memnew(Image);
	scaled = Image::create_empty(
		sprite->get_width() * 3, sprite->get_height() * 3, sprite->has_mipmaps(), sprite->get_format()
	);
	int width_minus_one = sprite->get_width() - 1;
	int height_minus_one = sprite->get_height() - 1;
	for (int x = 0; x < sprite->get_width(); x++)
	{
		for (int y = 0; y < sprite->get_height(); y++)
		{
			int xs = 3 * x;
			int ys = 3 * y;

			Color a = sprite->get_pixel(UtilityFunctions::UtilityFunctions::maxi(x - 1, 0), UtilityFunctions::maxi(y - 1, 0));
			Color b = sprite->get_pixel(UtilityFunctions::mini(x, width_minus_one), UtilityFunctions::maxi(y - 1, 0));
			Color c = sprite->get_pixel(UtilityFunctions::mini(x + 1, width_minus_one), UtilityFunctions::maxi(y - 1, 0));
			Color d = sprite->get_pixel(UtilityFunctions::maxi(x - 1, 0), UtilityFunctions::mini(y, height_minus_one));
			Color e = sprite->get_pixel(UtilityFunctions::mini(x, width_minus_one), UtilityFunctions::mini(y, height_minus_one));
			Color f = sprite->get_pixel(UtilityFunctions::mini(x + 1, width_minus_one), UtilityFunctions::mini(y, height_minus_one));
			Color g = sprite->get_pixel(UtilityFunctions::maxi(x - 1, 0), UtilityFunctions::mini(y + 1, height_minus_one));
			Color h = sprite->get_pixel(UtilityFunctions::mini(x, width_minus_one), UtilityFunctions::mini(y + 1, height_minus_one));
			Color i = sprite->get_pixel(UtilityFunctions::mini(x + 1, width_minus_one), UtilityFunctions::mini(y + 1, height_minus_one));

			bool db = SimilarColors(d, b, tol);
			bool dh = SimilarColors(d, h, tol);
			bool bf = SimilarColors(f, b, tol);
			bool ec = SimilarColors(e, c, tol);
			bool ea = SimilarColors(e, a, tol);
			bool fh = SimilarColors(f, h, tol);
			bool eg = SimilarColors(e, g, tol);
			bool ei = SimilarColors(e, i, tol);

			scaled->set_pixel(UtilityFunctions::maxi(xs - 1, 0), UtilityFunctions::maxi(ys - 1, 0), (db && !dh && !bf) ? d : e);
			scaled->set_pixel(
				xs,
				UtilityFunctions::maxi(ys - 1, 0),
				(db && !dh && !bf && !ec) || (bf && !db && !fh && !ea) ? b : e
			);
			scaled->set_pixel(xs + 1, UtilityFunctions::maxi(ys - 1, 0), (bf && !db && !fh) ? f : e);
			scaled->set_pixel(
				UtilityFunctions::maxi(xs - 1, 0),
				ys,
				((dh && !fh && !db && !ea) || (db && !dh && !bf && !eg)) ? d : e
			);
			scaled->set_pixel(xs, ys, e);
			scaled->set_pixel(
				xs + 1, ys, ((bf && !db && !fh && !ei) || (fh && !bf && !dh && !ec)) ? f : e
			);
			scaled->set_pixel(UtilityFunctions::maxi(xs - 1, 0), ys + 1, (dh && !fh && !db) ? d : e);
			scaled->set_pixel(
				xs, ys + 1, ((fh && !bf && !dh && !eg) || (dh && !fh && !db && !ei)) ? h : e
			);
			scaled->set_pixel(xs + 1, ys + 1, (fh && !bf && !dh) ? f : e);
		}
	}

	return scaled;
}

void DrawingAlgosCpp::Transform(Ref<Image> image, Dictionary params, RotationAlgorithm algorithm, bool expand)
{
	Transform2D transformation_matrix = params.get("transformation_matrix", Transform2D());
	Vector2 pivot = params.get("pivot", image->get_size() / 2);

	if (expand)
	{
		Rect2i image_rect = Rect2i(Vector2(0, 0), image->get_size());
		Rect2i new_image_rect = transformation_matrix.xform(image_rect);
		Vector2i new_image_size = new_image_rect.size;
		if (image->get_size() != new_image_size)
		{
			pivot = new_image_size / 2 - (Vector2i(pivot) - image->get_size() / 2);
			Ref<Image> tmp_image = memnew(Image);
			tmp_image->create_empty(
				new_image_size.x, new_image_size.y, image->has_mipmaps(), image->get_format()
			);
			tmp_image->blit_rect(image, image_rect, (new_image_size - image->get_size()) / 2);
			image->copy_from(tmp_image);
		}
	}

	if (TypeIsShader(algorithm))
	{
		params["pivot"] = pivot / Vector2(image->get_size());
		Ref<Resource> shader = rotxel_shader;
		switch (algorithm)
		{
			case RotationAlgorithm::CLEANEDGE:
				shader = clean_edge_shader;
				break;
			
			case RotationAlgorithm::OMNISCALE:
				shader = omniscale_shader;
				break;
			
			case RotationAlgorithm::NNS:
				shader = nn_shader;
				break;
		}

		Ref<RefCounted> gen = memnew(RefCounted);
		gen->set_script("res://src/Classes/ShaderImageEffect.gd");
		gen->call("generate_image", image, shader, params, image->get_size());
	}
	else
	{
		real_t angle = transformation_matrix.get_rotation();
		switch (algorithm)
		{
			case RotationAlgorithm::ROTXEL:
				Rotxel(image, angle, pivot);
				break;
			
			case RotationAlgorithm::NN:
				NNRotate(image, angle, pivot);
				break;
			
			case RotationAlgorithm::URD:
				FakeRotsprite(image, angle, pivot);
				break;
		}
	}
}

bool DrawingAlgosCpp::TypeIsShader(RotationAlgorithm algorithm)
{
	return algorithm <= RotationAlgorithm::NNS;
}

Rect2 DrawingAlgosCpp::TransformRectangle(Rect2 rect, Transform2D matrix, Vector2 pivot)
{
	pivot = rect.size / 2;

	Rect2 offset_rect = rect;
	Vector2 offset_pos = -pivot;

	offset_rect.position = offset_pos;
	offset_rect = matrix.xform(offset_rect);
	offset_rect.position = rect.position + offset_rect.position - offset_pos;

    return offset_rect;
}

void DrawingAlgosCpp::Rotxel(Ref<Image> sprite, float angle, Vector2 pivot)
{
	if (UtilityFunctions::is_zero_approx(angle) || UtilityFunctions::is_equal_approx(angle, Math_TAU))
		return;
	
	if (UtilityFunctions::is_equal_approx(angle, Math_PI / 2.0) || UtilityFunctions::is_equal_approx(angle, 3.0 * Math_PI / 2.0))
	{
		NNRotate(sprite, angle, pivot);
		return;
	}

	if (UtilityFunctions::is_equal_approx(angle, Math_PI))
	{
		sprite->rotate_180();
		return;
	}

	Ref<Image> aux = memnew(Image);
	aux->copy_from(sprite);

	int ox = 0;
	int oy = 0;
	for (int x = 0; x < sprite->get_size().x; x++)
	{
		for (int y = 0; y < sprite->get_size().y; y++)
		{
			int dx = 3 * (x - pivot.x);
			int dy = 3 * (y - pivot.y);
			bool found_pixel = false;

			for (int k = 0; k < 9; k++)
			{
				int modk = -1 + k % 3;
				int divk = -1 + static_cast<int>(k / 3);
				double dir = Math::atan2(static_cast<double>(dy + divk), static_cast<double>(dx + modk));
				double mag = Math::sqrt(
					Math::pow(dx + modk, 2.0)
					+ Math::pow(dy + divk, 2.0)
				);

				dir -= angle;
				ox = UtilityFunctions::roundi(pivot.x * 3 + 1 + mag * cos(dir));
				oy = UtilityFunctions::roundi(pivot.y * 3 + 1 + mag * sin(dir));

				if (sprite->get_width() % 2 != 0)
				{
					ox += 1;
					oy += 1;
				}

				if (ox >= 0 && ox < sprite->get_width() *3 && oy >= 0 && oy < sprite->get_height() * 3)
				{
					found_pixel = true;
					break;
				}
			}

			if (!found_pixel)
			{
				sprite->set_pixel(x, y, Color(0, 0, 0, 0));
				continue;
			}

			int fil = oy % 3;
			int col = ox % 3;
			int index = col + 3 * fil;

			ox = UtilityFunctions::roundi((ox - 1) / 3.0);
			oy = UtilityFunctions::roundi((oy - 1) / 3.0);
			
			Color p;
			if (ox == 0 || ox == sprite->get_width() - 1 || oy == 0 || oy == sprite->get_height() - 1)
				p = aux->get_pixel(ox, oy);
			else
			{
				Color a = aux->get_pixel(ox - 1, oy - 1);
				Color b = aux->get_pixel(ox, oy - 1);
				Color c = aux->get_pixel(ox + 1, oy - 1);
				Color d = aux->get_pixel(ox - 1, oy);
				Color e = aux->get_pixel(ox, oy);
				Color f = aux->get_pixel(ox + 1, oy);
				Color g = aux->get_pixel(ox - 1, oy + 1);
				Color h = aux->get_pixel(ox, oy + 1);
				Color i = aux->get_pixel(ox + 1, oy + 1);

				switch (index)
				{
					case 0:
						p = (SimilarColors(d, b) && !SimilarColors(d, h) && !SimilarColors(b, f)) ? d : e;
						break;
					case 1:
						p = ((SimilarColors(d, b) && !SimilarColors(d, h) && !SimilarColors(b, f) && !SimilarColors(e, c)) ||
							(SimilarColors(b, f) && !SimilarColors(d, b) && !SimilarColors(f, h) && !SimilarColors(e, a))) ? b : e;
						break;
					case 2:
						p = (SimilarColors(b, f) && !SimilarColors(d, b) && !SimilarColors(f, h)) ? f : e;
						break;
					case 3:
						p = ((SimilarColors(d, h) && !SimilarColors(f, h) && !SimilarColors(d, b) && !SimilarColors(e, a)) ||
							(SimilarColors(d, b) && !SimilarColors(d, h) && !SimilarColors(b, f) && !SimilarColors(e, g))) ? d : e;
						break;
					case 4:
						p = e;
						break;
					case 5:
						p = ((SimilarColors(b, f) && !SimilarColors(d, b) && !SimilarColors(f, h) && !SimilarColors(e, i)) ||
							(SimilarColors(f, h) && !SimilarColors(b, f) && !SimilarColors(d, h) && !SimilarColors(e, c))) ? f : e;
						break;
					case 6:
						p = (SimilarColors(d, h) && !SimilarColors(f, h) && !SimilarColors(d, b)) ? d : e;
						break;
					case 7:
						p = ((SimilarColors(f, h) && !SimilarColors(f, b) && !SimilarColors(d, h) && !SimilarColors(e, g)) ||
							(SimilarColors(d, h) && !SimilarColors(f, h) && !SimilarColors(d, b) && !SimilarColors(e, i))) ? h : e;
						break;
					case 8:
						p = (SimilarColors(f, h) && !SimilarColors(f, b) && !SimilarColors(d, h)) ? f : e;
						break;
				}
			}

			sprite->set_pixel(x, y, p);
		}
	}
}

void DrawingAlgosCpp::FakeRotsprite(Ref<Image> sprite, float angle, Vector2 pivot)
{
	if (UtilityFunctions::is_zero_approx(angle) || UtilityFunctions::is_equal_approx(angle, Math_TAU))
		return;
	
	if (UtilityFunctions::is_equal_approx(angle, Math_PI/ 2.0) || UtilityFunctions::is_equal_approx(angle, 2.0 * Math_PI / 2.0))
	{
		NNRotate(sprite, angle, pivot);
		return;
	}

	if (UtilityFunctions::is_equal_approx(angle, Math_PI))
	{
		sprite->rotate_180();
		return;
	}

	Ref<Image> selected_sprite = Scale3x(sprite);
	NNRotate(selected_sprite, angle, pivot * 3);
	selected_sprite->resize(
		selected_sprite->get_width() / 3, selected_sprite->get_height() / 3, selected_sprite->INTERPOLATE_NEAREST
	);
	sprite->blit_rect(selected_sprite, Rect2(Vector2(0, 0), selected_sprite->get_size()), Vector2(0, 0));
}

void DrawingAlgosCpp::NNRotate(Ref<Image> sprite, float angle, Vector2 pivot)
{
	if (UtilityFunctions::is_zero_approx(angle) || UtilityFunctions::is_equal_approx(angle, Math_TAU))
		return;
	
	if (UtilityFunctions::is_equal_approx(angle, Math_PI))
	{
		sprite->rotate_180();
		return;
	}

	Ref<Image> aux = memnew(Image);
	aux->copy_from(sprite);
	
	double angle_sin = Math::sin(angle);
	double angle_cos = Math::cos(angle);
	for (int x = 0; x < sprite->get_width(); x++)
	{
		for (int y = 0; y < sprite->get_width(); y++)
		{
			int ox = (x - pivot.x) * angle_cos + (y - pivot.y) * angle_sin + pivot.x;
			int oy = -(x - pivot.x) * angle_sin + (y - pivot.y) * angle_cos + pivot.y;

			if (ox >= 0 && ox < sprite->get_width() && oy >= 0 && oy < sprite->get_height())
				sprite->set_pixel(x, y, aux->get_pixel(ox, oy));
			else
				sprite->set_pixel(x, y, Color(0, 0, 0, 0));
		}
	}
}

bool DrawingAlgosCpp::SimilarColors(Color c1, Color c2, float tol)
{
	return 
		UtilityFunctions::absf(c1.r - c2.r) <= tol
		&& UtilityFunctions::absf(c1.g - c2.g) <= tol
		&& UtilityFunctions::absf(c1.b - c2.b) <= tol
		&& UtilityFunctions::absf(c1.a - c2.a) <= tol;
}

void DrawingAlgosCpp::Center(Array indices)
{
	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");
	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");

  	Dictionary redo_data, undo_data;
  	project->set("undos", static_cast<int>(project->get("undos")) + 1);

  	UndoRedo *undo_redo = Object::cast_to<UndoRedo>(project->get("undo_redo"));
  	undo_redo->create_action("Center Frames");
	
  	for (const int32_t & frame : indices)
	{
		Rect2i used_rect = Rect2i();
		
		Array project_frames = project->get("frames");
		Array cels = project_frames[frame].get("cels");

		for (int i = 0; i < cels.size(); i++)
		{
			Ref<RefCounted> cel = Object::cast_to<RefCounted>(cels[i]);
			if (cel->get_script() != "res://src/Classes/Cels/PixelCel.gd")
				continue;
			
			Rect2i cel_rect = cel->call("get_image").call("get_used_rect");
			if (cel_rect.has_area())
				used_rect = (used_rect.has_area()) ? used_rect.merge(cel_rect) : cel_rect;
		}

		if (!used_rect.has_area())
			continue;
		
		Vector2i project_size = project->get("size");
		Vector2i offset = static_cast<Vector2>(0.5 * (project_size - used_rect.size)).floor();

		for (int i = 0; i < cels.size(); i++)
		{
			Ref<RefCounted> cel = Object::cast_to<RefCounted>(cels[i]);
			if (cel->get_script() != "res://src/Classes/Cels/PixelCel.gd")
				continue;
			
			Ref<Image> cel_image = cel->call("get_image");
			cel_image->set_script("res://src/Classes/ImageExtended.gd");

			Ref<Image> tmp_centered = project->call("new_empty_image");
			tmp_centered->blend_rect(cel->get("image"), used_rect, offset);

			Ref<Image> centered = memnew(Image);
			centered->set_script("res://src/Classes/ImageExtended.gd");
			centered->call("copy_from_custom", tmp_centered, cel_image->get("is_indexed"));

			if (cel->get_script() == "res://src/Classes/Cels/CelTileMap.gd")
				cel->call("serialize_undo_data_source_image", centered, redo_data, undo_data);
			
			centered->call("add_data_to_dictionary", redo_data, undo_data);
			cel_image->call("add_data_to_dictionary", undo_data);
		}
	}

	project->call("deserialize_cel_undo_data", redo_data, undo_data);

	Variant undo_or_redo_variant = global->get("undo_or_redo");
	Callable undo_or_redo_callable = undo_or_redo_variant;
	
	undo_redo->call("add_undo_method", undo_or_redo_callable.bind(true));
	undo_redo->call("add_do_method", undo_or_redo_callable.bind(false));

	undo_redo->call("commit_action");
}

void DrawingAlgosCpp::ScaleProject(int width, int height, int interpolation)
{
	Dictionary redo_data, undo_data;

	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");
	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");

	TypedArray<RefCounted> pixel_cels = project->call("get_all_pixel_cels");

	for (int i = 0; i < pixel_cels.size(); i++)
	{
		Ref<RefCounted> cel = pixel_cels[i];
		cel->set_script("res://src/Classes/Cels/PixelCel.gd"); // TODO: This assigns PixeCel to everything, but what if the cel is no that?
		
		Ref<Image> cel_image = cel->call("get_image");
		cel_image->set_script("res://src/Classes/ImageExtended.gd");
		
		Ref<Image> sprite = ResizeImage(cel_image, width, height, interpolation);
		sprite->set_script("res://src/Classes/ImageExtended.gd");

		if (cel->get_script() == "res://src/Classes/Cels/CelTileMap.gd")
			cel->call("serialize_undo_data_source_image", sprite, redo_data, undo_data);
		
 		sprite->call("add_data_to_dictionary", redo_data, cel_image);
 		cel_image->call("add_data_to_dictionary", undo_data);
	}

	GeneralDoAndUndoScale(width, height, redo_data, undo_data);
}

Ref<Image> DrawingAlgosCpp::ResizeImage(Ref<Image> image, int width, int height, int interpolation)
{
	Ref<Image> new_image = memnew(Image);
	if (image->get_script() == "res://src/Classes/ImageExtended.gd")
	{
		new_image->set_script("res://src/Classes/ImageExtended.gd");
		new_image->set("is_indexed", image->get("is_indexed"));
		new_image->copy_from(image);
		new_image->call("select_palette", "", false);
	}
	else
		new_image->copy_from(image);

	if (interpolation == Interpolation::SCALE3X_INTERPOLATION)
	{
		Vector2i times = Vector2i(
			UtilityFunctions::ceili(width / (3.0 * new_image->get_width())),
			UtilityFunctions::ceili(height / (3.0 * new_image->get_height()))
		);
		
		for (int i = 0; i < UtilityFunctions::maxi(times.x, times.y); i++)
			new_image->copy_from(Scale3x(new_image));
		new_image->resize(width, height, Image::Interpolation::INTERPOLATE_NEAREST);
	}
	else if (interpolation == Interpolation::CLEANEDGE_INTERPOLATION)
	{
		Ref<RefCounted> gen = memnew(RefCounted);
		gen->set_script("res://src/Classes/ShaderImageEffect.gd");

		gen->call("generate_image", new_image, clean_edge_shader, Dictionary(),  Vector2i(width, height), false);
	}
	else if (interpolation == Interpolation::OMNISCALE_INTERPOLATION) // && omniscale_shader) // TODO: What does this check for and how can we do it in C++?
	{
		Ref<RefCounted> gen = memnew(RefCounted);
		gen->set_script("res://src/Classes/ShaderImageEffect.gd");

		gen->call("generate_image", new_image, omniscale_shader, Dictionary(),  Vector2i(width, height), false);
	}
	else
		new_image->resize(width, height, static_cast<Image::Interpolation>(interpolation));
	
	if (new_image->get_script() == "res://src/Classes/ImageExtended.gd")
		new_image->call("on_size_changed");
	
	return new_image;
}

void DrawingAlgosCpp::CropToSelection()
{
	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");

	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");

	if (!project->get("has_selection"))
		return;
	
	Node2D *canvas = Object::cast_to<Node2D>(global->get("canvas"));
	project->set_script("res://src/UI/Canvas/Canvas.gd");

	Node2D *selection = Object::cast_to<Node2D>(canvas->get("canvas"));
	selection->set_script("res://src/UI/Canvas/SelectionNode");

	selection->call("transform_content_confirm");
	
	Dictionary redo_data, undo_data;
	Rect2i rect = selection->get("big_bounding_rectangle");

	TypedArray<RefCounted> pixel_cels = project->call("get_all_pixel_cels");

	for (int i = 0; i < pixel_cels.size(); i++)
	{
		Ref<RefCounted> cel = pixel_cels[i];
		cel->set_script("res://src/Classes/Cels/PixelCel.gd"); // TODO: This assigns PixeCel to everything, but what if the cel is no that?
		
		Ref<Image> cel_image = cel->call("get_image");
		cel_image->set_script("res://src/Classes/ImageExtended.gd");
		
		Ref<Image> tmp_cropped = cel_image->get_region(rect);
		
		Ref<Image> cropped = memnew(Image);
		cropped->set_script("res://src/Classes/ImageExtended.gd");

		cropped->call("copy_from_custom", tmp_cropped, static_cast<bool>(cel_image->get("is_indexed")));

		if (cel->get_script() == "res://src/Classes/Cels/CelTileMap.gd")
			cel->call("serialize_undo_data_source_image", cropped, redo_data, undo_data);
		
		cropped->call("add_data_to_dictionary", redo_data, cel_image);
 		cel_image->call("add_data_to_dictionary", undo_data);
	}

	GeneralDoAndUndoScale(rect.size.x, rect.size.y, redo_data, undo_data);
}

void DrawingAlgosCpp::CropToContent()
{
	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");

	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");
	
	Node2D *canvas = Object::cast_to<Node2D>(global->get("canvas"));
	project->set_script("res://src/UI/Canvas/Canvas.gd");

	Node2D *selection = Object::cast_to<Node2D>(canvas->get("canvas"));
	selection->set_script("res://src/UI/Canvas/SelectionNode");

	selection->call("transform_content_confirm");
	
	Rect2i used_rect;

	TypedArray<RefCounted> pixel_cels = project->call("get_all_pixel_cels");

	for (int i = 0; i < pixel_cels.size(); i++)
	{
		Ref<RefCounted> cel = pixel_cels[i];
		cel->set_script("res://src/Classes/Cels/PixelCel.gd"); // TODO: This assigns PixeCel to everything, but what if the cel is no that?

		Ref<Image> cel_image = cel->call("get_image");
		cel_image->set_script("res://src/Classes/ImageExtended.gd");
		
		Rect2i cel_used_rect = cel_image->get_used_rect();
		if (cel_used_rect == Rect2(0, 0, 0, 0))
			continue;
		
		if (used_rect == Rect2i(0, 0, 0, 0))
			used_rect = cel_used_rect;
		else
			used_rect = used_rect.merge(cel_used_rect);
	}

	if (used_rect == Rect2i(0, 0, 0, 0))
		return;
	
	int width = used_rect.size.x;
	int height = used_rect.size.y;
	Dictionary redo_data, undo_data;

	for (int i = 0; i < pixel_cels.size(); i++)
	{
		Ref<RefCounted> cel = pixel_cels[i];
		cel->set_script("res://src/Classes/Cels/PixelCel.gd"); // TODO: This assigns PixeCel to everything, but what if the cel is no that?

		Ref<Image> cel_image = cel->call("get_image");
		cel_image->set_script("res://src/Classes/ImageExtended.gd");
		 
		Ref<Image> tmp_cropped = cel_image->get_region(used_rect);
		
		Ref<Image> cropped = memnew(Image);
		cropped->set_script("res://src/Classes/ImageExtended.gd");

		cropped->call("copy_from_custom", tmp_cropped, static_cast<bool>(cel_image->get("is_indexed")));

		if (cel->get_script() == "res://src/Classes/Cels/CelTileMap.gd")
			cel->call("serialize_undo_data_source_image", cropped, redo_data, undo_data);
		
		cropped->call("add_data_to_dictionary", redo_data, cel_image);
 		cel_image->call("add_data_to_dictionary", undo_data);
	}

	GeneralDoAndUndoScale(width, height, redo_data, undo_data);
}

void DrawingAlgosCpp::ResizeCanvas(int width, int height, int offset_x, int offset_y)
{
	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");

	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");
	
	Dictionary redo_data, undo_data;

	TypedArray<RefCounted> pixel_cels = project->call("get_all_pixel_cels");

	for (int i = 0; i < pixel_cels.size(); i++)
	{
		Ref<RefCounted> cel = pixel_cels[i];
		cel->set_script("res://src/Classes/Cels/PixelCel.gd"); // TODO: This assigns PixeCel to everything, but what if the cel is no that?

		Ref<Image> cel_image = cel->call("get_image");
		cel_image->set_script("res://src/Classes/ImageExtended.gd");
		
		Ref<Image> resized = memnew(Image);
		resized->set_script("res://src/Classes/ImageExtended.gd");
		
		resized->call("create_custom", width, height, cel_image->has_mipmaps(), cel_image->get_format(), cel_image->get("is_indexed"));
		resized->blend_rect(cel_image, Rect2i(Vector2i(0, 0), cel_image->get_size()), Vector2i(offset_x, offset_y));
		resized->call("convert_rgb_to_indexed");

		if (cel->get_script() == "res://src/Classes/Cels/CelTileMap.gd")
		{
			Ref<RefCounted> tileset = cel->get("tileset");
			Vector2i tile_size = tileset->get("tile_size");

			bool skip_tileset = (offset_x % tile_size.x == 0 && offset_y % tile_size.y == 0);
			cel->call("serialize_undo_data_source_image", resized, redo_data, undo_data, skip_tileset);
		}

		resized->call("add_data_to_dictionary", redo_data, cel_image);
		cel_image->call("add_data_to_dictionary", undo_data);
	}

	GeneralDoAndUndoScale(width, height, redo_data, undo_data);
}

void DrawingAlgosCpp::GeneralDoAndUndoScale(int width, int height, Dictionary redo_data, Dictionary undo_data)
{
	SceneTree *scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	Node *global = scene_tree->get_root()->get_node_or_null("Global");
	Ref<RefCounted> project = Object::cast_to<RefCounted>(global->get("current_project"));
	project->set_script("res://src/Classes/Project.gd");

	Vector2i size(width, height);

	Vector2i project_size = project->get("size");
	float x_ratio = project_size.x / width;
	float y_ratio = project_size.y / height;
	
	Ref<RefCounted> selection_map_copy = memnew(RefCounted);
	selection_map_copy->set_script("res://src/classes/SelectionMap.gd");
	selection_map_copy->call("copy_from", project->get("selection_map"));
	selection_map_copy->call("crop", size.x, size.y);
	redo_data[project->get("selection_map")] = selection_map_copy->get("data");
	undo_data[project->get("selection_map")] = Object::cast_to<RefCounted>(project->get("selection_map"))->get("data");

	float new_x_symmetry_point = static_cast<float>(project->get("x_symmetry_point")) / x_ratio;
	float new_y_symmetry_point = static_cast<float>(project->get("y_symmetry_point")) / y_ratio;
	PackedVector2Array new_x_symmetry_axis_points = Object::cast_to<Line2D>(project->get("x_symmetry_axis"))->get_points();
	PackedVector2Array new_y_symmetry_axis_points = Object::cast_to<Line2D>(project->get("y_symmetry_axis"))->get_points();
	new_x_symmetry_axis_points[0].y /= y_ratio;
	new_x_symmetry_axis_points[1].y /= y_ratio;
	new_y_symmetry_axis_points[0].x /= x_ratio;
	new_y_symmetry_axis_points[1].x /= x_ratio;

	project->set("undos", static_cast<int>(project->get("undos")) + 1);

	UndoRedo *undo_redo = Object::cast_to<UndoRedo>(project->get("undo_redo"));
	undo_redo->create_action("Scale");
	undo_redo->add_do_property(*project, "size", size);
	undo_redo->add_do_property(*project, "x_symmetry_point", new_x_symmetry_point);
	undo_redo->add_do_property(*project, "y_symmetry_point", new_y_symmetry_point);
	undo_redo->add_do_property(project->get("x_symmetry_axis"), "points", new_x_symmetry_axis_points);
	undo_redo->add_do_property(project->get("y_symmetry_axis"), "points", new_y_symmetry_axis_points);
	project->call("deserialize_cel_undo_data",redo_data, undo_data);
	undo_redo->add_undo_property(*project, "size", project->get("size"));
	undo_redo->add_undo_property(*project, "x_symmetry_point", project->get("x_symmetry_point"));
	undo_redo->add_undo_property(*project, "y_symmetry_point", project->get("y_symmetry_point"));
	undo_redo->add_undo_property(
		project->get("x_symmetry_axis"), "points", Object::cast_to<Line2D>(project->get("x_symmetry_axis"))->get_points()
	);
	undo_redo->add_undo_property(
		project->get("y_symmetry_axis"), "points", Object::cast_to<Line2D>(project->get("y_symmetry_axis"))->get_points()
	);

	Variant undo_or_redo_variant = global->get("undo_or_redo");
	Callable undo_or_redo_callable = undo_or_redo_variant;

	undo_redo->add_undo_method(undo_or_redo_callable.bind(true));
	undo_redo->add_do_method(undo_or_redo_callable.bind(false));
	undo_redo->commit_action();
}
