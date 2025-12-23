/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#pragma once

#define blog(log_level, message, ...) blog(log_level, "[win_spout] " message, ##__VA_ARGS__)

#include <vector>
#include <string>

extern void spout_output_start(const char *SpoutName);
extern void spout_output_stop();

extern void spout_output_start(const char *canvasName, const char *SpoutName);
extern void spout_output_stop(const char *canvasName);
extern bool spout_output_is_active(const char *canvasName);
extern std::vector<std::string> get_canvas_names();
