/**************************************************************************/
/*  export_plugin.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "editor/editor_node.h"

Vector<String> EditorExportPlatformTVOS::device_types({ "AppleTV" });

void EditorExportPlatformTVOS::initialize() {
	if (EditorNode::get_singleton()) {
		EditorExportPlatformAppleEmbedded::_initialize(_tvos_logo_svg, _tvos_run_icon_svg);
#ifdef MACOS_ENABLED
		_start_remote_device_poller_thread();
#endif
	}
}

EditorExportPlatformTVOS::~EditorExportPlatformTVOS() {
#ifdef MACOS_ENABLED
	_stop_remote_device_poller_thread();
#endif
}

void EditorExportPlatformTVOS::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformAppleEmbedded::get_export_options(r_options);

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/min_tvos_version"), get_minimum_deployment_target()));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/tvos_small_app_icon", PROPERTY_HINT_FILE_PATH, "*.png,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/tvos_large_app_icon", PROPERTY_HINT_FILE_PATH, "*.png,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/tvos_top_shelf", PROPERTY_HINT_FILE_PATH, "*.png,*.jpg,*.jpeg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "icons/tvos_top_shelf_wide", PROPERTY_HINT_FILE_PATH, "*.png,*.jpg,*.jpeg"), ""));
}

bool EditorExportPlatformTVOS::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	bool valid = EditorExportPlatformAppleEmbedded::has_valid_export_configuration(p_preset, r_error, r_missing_templates, p_debug);

	String err;
	String rendering_method = get_project_setting(p_preset, "rendering/renderer/rendering_method.mobile");
	String rendering_driver = get_project_setting(p_preset, "rendering/rendering_device/driver." + get_platform_name());
	if ((rendering_method == "forward_plus" || rendering_method == "mobile") && rendering_driver == "metal") {
		float version = p_preset->get("application/min_tvos_version").operator String().to_float();
		if (version < 14.0) {
			err += TTR("Metal renderer require tvOS 14+.") + "\n";
		}
	}

	if (!err.is_empty()) {
		if (!r_error.is_empty()) {
			r_error += err;
		} else {
			r_error = err;
		}
	}

	return valid;
}

HashMap<String, Variant> EditorExportPlatformTVOS::get_custom_project_settings(const Ref<EditorExportPreset> &p_preset) const {
	return HashMap<String, Variant>();
}

Error EditorExportPlatformTVOS::_export_loading_screen_file(const Ref<EditorExportPreset> &p_preset, const String &p_dest_dir) {
	// tvOS does not require a custom storyboard configuration.
	return OK;
}

Error EditorExportPlatformTVOS::_export_icons(const Ref<EditorExportPreset> &p_preset, const String &p_iconset_dir) {
	struct IconEntry {
		String preset_key;
		String filename;
		int width = 0;
		int height = 0;
		String size_string;
		String scale_string;
		String role;
		bool require_opaque = false;
	};

	const IconEntry entries[] = {
		{ PNAME("icons/tvos_small_app_icon"), "AppIcon-400x240.png", 400, 240, "400x240", "1x", String(), true },
		{ PNAME("icons/tvos_large_app_icon"), "AppIcon-1280x768.png", 1280, 768, "1280x768", "1x", String(), true },
		{ PNAME("icons/tvos_top_shelf"), "TopShelf-1920x720.png", 1920, 720, "1920x720", "1x", "top shelf image", false },
		{ PNAME("icons/tvos_top_shelf_wide"), "TopShelfWide-2320x720.png", 2320, 720, "2320x720", "1x", "top shelf image wide", false },
	};

	String json_description = "{\"images\":[";
	String sizes;
	bool first_icon = true;

	if (DirAccess::open(p_iconset_dir).is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not open a directory at path \"%s\"."), p_iconset_dir));
		return ERR_CANT_OPEN;
	}

	Color boot_bg_color = get_project_setting(p_preset, "application/boot_splash/bg_color");

	for (const IconEntry &entry : entries) {
		String icon_path = p_preset->get(entry.preset_key);
		bool warn_resize = true;
		if (icon_path.is_empty()) {
			icon_path = get_project_setting(p_preset, "application/config/icon");
			warn_resize = false;
		}

		Error err = OK;
		Ref<Image> img = _load_icon_or_splash_image(icon_path, &err);
		if (err != OK || img.is_null() || img->is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Invalid icon (%s): '%s'.", entry.preset_key, icon_path));
			return ERR_UNCONFIGURED;
		}

		if (entry.require_opaque && img->detect_alpha() != Image::ALPHA_NONE) {
			if (warn_resize) {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Export Icons"), vformat("Icon (%s) must be opaque.", entry.preset_key));
			}
			Ref<Image> new_img = Image::create_empty(entry.width, entry.height, false, Image::FORMAT_RGBA8);
			new_img->fill(boot_bg_color);
			img->resize(entry.width, entry.height, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
			_blend_and_rotate(new_img, img, false);
			img = new_img;
		} else {
			if (img->get_width() != entry.width || img->get_height() != entry.height) {
				if (warn_resize) {
					add_message(EXPORT_MESSAGE_WARNING, TTR("Export Icons"), vformat("Icon (%s): '%s' has incorrect size %s and was automatically resized to %s.", entry.preset_key, icon_path, img->get_size(), Vector2i(entry.width, entry.height)));
				}
				img->resize(entry.width, entry.height, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
			}
		}

		String output_path = p_iconset_dir.path_join(entry.filename);
		err = img->save_png(output_path);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat("Failed to export icon (%s): '%s'.", entry.preset_key, output_path));
			return err;
		}

		if (first_icon) {
			first_icon = false;
		} else {
			json_description += ",";
		}

		json_description += "{";
		json_description += "\"idiom\":\"tv\",";
		json_description += "\"platform\":\"" + get_platform_name() + "\",";
		json_description += "\"size\":\"" + entry.size_string + "\",";
		json_description += "\"scale\":\"" + entry.scale_string + "\",";
		if (!entry.role.is_empty()) {
			json_description += "\"role\":\"" + entry.role + "\",";
		}
		json_description += "\"filename\":\"" + entry.filename + "\"";
		json_description += "}";

		sizes += itos(entry.width) + "x" + itos(entry.height) + "\n";
	}

	json_description += "],\"info\":{\"author\":\"xcode\",\"version\":1}}";

	Ref<FileAccess> json_file = FileAccess::open(p_iconset_dir + "Contents.json", FileAccess::WRITE);
	if (json_file.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not write to a file at path \"%s\"."), p_iconset_dir + "Contents.json"));
		return ERR_CANT_CREATE;
	}

	CharString json_utf8 = json_description.utf8();
	json_file->store_buffer((const uint8_t *)json_utf8.get_data(), json_utf8.length());

	Ref<FileAccess> sizes_file = FileAccess::open(p_iconset_dir + "sizes", FileAccess::WRITE);
	if (sizes_file.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export Icons"), vformat(TTR("Could not write to a file at path \"%s\"."), p_iconset_dir + "sizes"));
		return ERR_CANT_CREATE;
	}

	CharString sizes_utf8 = sizes.utf8();
	sizes_file->store_buffer((const uint8_t *)sizes_utf8.get_data(), sizes_utf8.length());

	return OK;
}

String EditorExportPlatformTVOS::_process_config_file_line(const Ref<EditorExportPreset> &p_preset, const String &p_line, const AppleEmbeddedConfigData &p_config, bool p_debug, const CodeSigningDetails &p_code_signing) {
	String strnew;

	if (p_line.contains("$targeted_device_family")) {
		strnew += p_line.replace("$targeted_device_family", "3") + "\n";

	} else if (p_line.contains("$moltenvk_buildfile")) {
		strnew += p_line.replace("$moltenvk_buildfile", "") + "\n";
	} else if (p_line.contains("$moltenvk_fileref")) {
		strnew += p_line.replace("$moltenvk_fileref", "") + "\n";
	} else if (p_line.contains("$moltenvk_buildphase")) {
		strnew += p_line.replace("$moltenvk_buildphase", "") + "\n";
	} else if (p_line.contains("$moltenvk_buildgrp")) {
		strnew += p_line.replace("$moltenvk_buildgrp", "") + "\n";

	} else if (p_line.contains("$plist_launch_screen_name")) {
		strnew += p_line.replace("$plist_launch_screen_name", "") + "\n";
	} else if (p_line.contains("$pbx_launch_screen_file_reference")) {
		strnew += p_line.replace("$pbx_launch_screen_file_reference", "") + "\n";
	} else if (p_line.contains("$pbx_launch_screen_copy_files")) {
		strnew += p_line.replace("$pbx_launch_screen_copy_files", "") + "\n";
	} else if (p_line.contains("$pbx_launch_screen_build_phase")) {
		strnew += p_line.replace("$pbx_launch_screen_build_phase", "") + "\n";
	} else if (p_line.contains("$pbx_launch_screen_build_reference")) {
		strnew += p_line.replace("$pbx_launch_screen_build_reference", "") + "\n";
	} else if (p_line.contains("$launch_screen_image_mode")) {
		strnew += p_line.replace("$launch_screen_image_mode", "") + "\n";
	} else if (p_line.contains("$launch_screen_background_color")) {
		strnew += p_line.replace("$launch_screen_background_color", "") + "\n";

	} else if (p_line.contains("$os_deployment_target")) {
		String min_version = p_preset->get("application/min_" + get_platform_name() + "_version");
		String value = "TVOS_DEPLOYMENT_TARGET = " + min_version + ";";
		strnew += p_line.replace("$os_deployment_target", value) + "\n";

	} else if (p_line.contains("$valid_archs")) {
		strnew += p_line.replace("$valid_archs", "arm64 x86_64") + "\n";

	} else {
		strnew += EditorExportPlatformAppleEmbedded::_process_config_file_line(p_preset, p_line, p_config, p_debug, p_code_signing);
	}

	return strnew;
}
