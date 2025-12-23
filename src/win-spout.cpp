/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QMessageBox>

#include "win-spout.h"
#include "ui/win-spout-output-settings.h"
#include "win-spout-config.h"

#include <map>
#include <string>

// Global map for multi-canvas outputs
static std::map<std::string, obs_output_t *> active_outputs;

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Off World Live")
OBS_MODULE_USE_DEFAULT_LOCALE("win-spout", "en-US")

extern obs_source_info create_spout_source_info();
obs_source_info spout_source_info;

extern obs_output_info create_spout_output_info();
obs_output_info spout_output_info;

extern obs_source_info create_spout_filter_info();
obs_source_info spout_filter_info;

win_spout_output_settings *spout_output_settings;
obs_output_t *win_spout_out;

static void on_obs_frontend_event(obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		// Auto-start outputs after OBS is fully loaded
		auto *config = win_spout_config::get();
#if SUPPORTS_MULTI_CANVAS
		// Debug: Log available canvases
		std::vector<std::string> availableCanvases = get_canvas_names();
		blog(LOG_INFO, "OBS finished loading. Available canvases: %d", (int)availableCanvases.size());
		for (const auto &name : availableCanvases) {
			blog(LOG_INFO, "  Canvas: '%s'", name.c_str());
		}
		
		for (const auto &[canvasName, spoutName, autoStart] : config->outputs) {
			if (autoStart && !canvasName.isEmpty() && !spoutName.isEmpty()) {
				blog(LOG_INFO, "Auto-starting Spout output: %s -> %s",
				     canvasName.toUtf8().constData(), spoutName.toUtf8().constData());
				spout_output_start(canvasName.toUtf8().constData(),
						   spoutName.toUtf8().constData());
			}
		}
#else
		if (config->auto_start) {
			blog(LOG_INFO, "Auto-starting legacy Spout output");
			spout_output_start(config->spout_output_name.toUtf8().constData());
		}
#endif
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
#if SUPPORTS_MULTI_CANVAS
		// Stop and release all multi-canvas outputs
		for (auto &[key, output] : active_outputs) {
			if (output) {
				obs_output_stop(output);
				obs_output_release(output);
			}
		}
		active_outputs.clear();
#else
		// Legacy output cleanup
		if (win_spout_out) {
			obs_output_stop(win_spout_out);
			obs_output_release(win_spout_out);
			win_spout_out = nullptr;
		}
#endif
	}
}

bool obs_module_load()
{
	// register input source type
	spout_source_info = create_spout_source_info();
	obs_register_source(&spout_source_info);

	// register filter source type
	spout_filter_info = create_spout_filter_info();
	obs_register_source(&spout_filter_info);

	auto *config = win_spout_config::get();
	config->load();

	// register output type
	spout_output_info = create_spout_output_info();
	obs_register_output(&spout_output_info);

	// create an output instance of our output type we registered above
	obs_data_t *settings = obs_data_create();
	win_spout_out = obs_output_create("spout_output", "OBS Spout Output", settings, nullptr);
	obs_data_release(settings);

	obs_frontend_add_event_callback(on_obs_frontend_event, nullptr);

	// ui stuff
	{
		auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (!main_window) {
			blog(LOG_ERROR, "Can't get main window!");
			return false;
		}

		const auto *menu_action =
			static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("toolslabel")));

		obs_frontend_push_ui_translation(obs_module_get_string);
		spout_output_settings = new win_spout_output_settings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] {
			spout_output_settings->toggle_show_hide();
		};
		QAction::connect(menu_action, &QAction::triggered, menu_cb);
	}

	blog(LOG_INFO, "win-spout loaded!");

	return true;
}

void obs_module_unload()
{
#if SUPPORTS_MULTI_CANVAS
	// Safety cleanup - stop and release any remaining outputs
	for (auto &[key, output] : active_outputs) {
		if (output) {
			obs_output_stop(output);
			obs_output_release(output);
		}
	}
	active_outputs.clear();
#else
	// Legacy cleanup
	if (win_spout_out) {
		obs_output_stop(win_spout_out);
		obs_output_release(win_spout_out);
		win_spout_out = nullptr;
	}
#endif

	blog(LOG_INFO, "win-spout unloaded!");
}

const char *obs_module_name()
{
	return "win-spout";
}

const char *obs_module_description()
{
	return "Spout input/output for OBS Studio";
}

void spout_output_start(const char *SpoutName)
{
	// Legacy Single Output Implementation
	if (win_spout_out) {
		obs_data_t *settings = obs_output_get_settings(win_spout_out);
		obs_data_set_string(settings, "senderName", SpoutName);
		obs_output_update(win_spout_out, settings);
		obs_data_release(settings);
		obs_output_start(win_spout_out);
	}
}

void spout_output_stop()
{
	// Legacy Single Output Implementation
	if (win_spout_out) {
		obs_output_stop(win_spout_out);
	}
}

void spout_output_start(const char *canvasName, const char *SpoutName)
{
#if SUPPORTS_MULTI_CANVAS
	const std::string key = canvasName;

	obs_output_t *output = nullptr;
	if (const auto it = active_outputs.find(key); it != active_outputs.end()) {
		output = it->second;
	} else {
		obs_data_t *settings = obs_data_create();
		output = obs_output_create("spout_output", SpoutName, settings, nullptr);
		obs_data_release(settings);
		if (output) {
			active_outputs[key] = output;
			blog(LOG_INFO, "Created Spout output for canvas '%s' with sender '%s'", canvasName, SpoutName);
		} else {
			blog(LOG_ERROR, "Failed to create Spout output for canvas '%s'", canvasName);
			return;
		}
	}

	if (output) {
		// Update Sender Name
		obs_data_t *settings = obs_output_get_settings(output);
		obs_data_set_string(settings, "senderName", SpoutName);
		obs_output_update(output, settings);
		obs_data_release(settings);

		// Set Canvas
		video_t *video = nullptr;
		if (obs_canvas_t *canvas = obs_get_canvas_by_name(canvasName)) {
			video = obs_canvas_get_video(canvas);
			obs_canvas_release(canvas);
			blog(LOG_INFO, "Found canvas '%s' by name", canvasName);
		}

		if (video) {
			obs_output_set_media(output, video, obs_get_audio());
			
			if (!obs_output_start(output)) {
				blog(LOG_ERROR, "Failed to start Spout output for canvas '%s'", canvasName);
				QMessageBox::critical(nullptr, "Spout Output Error",
					QString("Failed to start Spout output for canvas '%1'.\n\n"
						"The output could not be started.")
					.arg(canvasName));
			}
		} else {
			blog(LOG_ERROR, "Canvas '%s' not found or has no video", canvasName);
			QMessageBox::critical(nullptr, "Spout Output Error",
				QString("Cannot start Spout output for canvas '%1'.\n\n"
					"The canvas was not found or its video is not available.\n"
					"Please check that the canvas exists and is properly configured.")
				.arg(canvasName));
			
			// Clean up the output we created since we can't use it
			obs_output_release(output);
			active_outputs.erase(key);
			return;
		}
	}
#else
	(void)canvasName;
	spout_output_start(SpoutName);
#endif
}

void spout_output_stop(const char *canvasName)
{
#if SUPPORTS_MULTI_CANVAS
	const std::string key = canvasName;
	if (const auto it = active_outputs.find(key); it != active_outputs.end()) {
		obs_output_stop(it->second);
		obs_output_release(it->second);
		active_outputs.erase(it);
	}
#else
	(void)canvasName;
	spout_output_stop();
#endif
}

bool spout_output_is_active(const char *canvasName)
{
#if SUPPORTS_MULTI_CANVAS
	const std::string key = canvasName;
	if (auto it = active_outputs.find(key); it != active_outputs.end()) {
		return obs_output_active(it->second);
	}
	return false;
#else
	(void)canvasName;
	return win_spout_out && obs_output_active(win_spout_out);
#endif
}

#if SUPPORTS_MULTI_CANVAS
// Helper struct for canvas search
struct CanvasSearchData {
	const char *searchName;
	obs_canvas_t *foundCanvas;
};

// Helper callback for finding canvas by name via enumeration
static bool find_canvas_proc(void *data, obs_canvas_t *canvas)
{
	auto *search = static_cast<CanvasSearchData *>(data);
	if (const char *name = obs_canvas_get_name(canvas)) {
		if (strcmp(name, search->searchName) == 0) {
			search->foundCanvas = canvas;
			// Don't release here - caller will use it
			return false; // Stop enumeration
		}
	}
	return true; // Continue enumeration
}

// Find canvas by enumeration (fallback when obs_get_canvas_by_name fails)
static obs_canvas_t *find_canvas_by_enumeration(const char *canvasName)
{
	CanvasSearchData data = {canvasName, nullptr};
	obs_enum_canvases(find_canvas_proc, &data);
	return data.foundCanvas;
}

// Helper callback for getting canvas names
static bool enum_canvases_proc(void *data, obs_canvas_t *canvas)
{
	auto *names = static_cast<std::vector<std::string> *>(data);
	if (const char *name = obs_canvas_get_name(canvas)) {
		names->emplace_back(name);
	}
	return true;
}
#endif

std::vector<std::string> get_canvas_names()
{
	std::vector<std::string> names;
#if SUPPORTS_MULTI_CANVAS
	obs_enum_canvases(enum_canvases_proc, &names);

	// If no canvases found (or main not returned), might want to ensure Defaults are there?
	// But let's trust the enumeration.
	// If list is empty, we might want to fallback or add "Main" if we know it's not covered?
	// Usually "Main" is not an obs_canvas_t in the same way? Or maybe it is?
	// Let's add "Main" manually if it's missing, or just rely on user typing it if it's special.
	// For safety/UX, let's keep "Main" as a suggestion if the list is empty,
	// or Prepend it if we think it's separate.
	// Assuming obs_enum_canvases covers created viewports.
	if (names.empty()) {
		names.emplace_back("Main");
	}
#else
	names.emplace_back("Main");
#endif
	return names;
}
