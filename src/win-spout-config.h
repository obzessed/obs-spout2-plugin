/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#pragma once

#include <QString>
#include <obs.h>

// Version check macro
#ifndef MAKE_SEMANTIC_VERSION
#define MAKE_SEMANTIC_VERSION(major, minor, patch) ((major << 24) | (minor << 16) | (patch))
#endif

// Multi-Canvas Support (OBS >= 31.1.0)
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(31, 1, 0)
#define SUPPORTS_MULTI_CANVAS 1
#else
#define SUPPORTS_MULTI_CANVAS 0
#endif

struct SpoutOutputConfig {
	QString canvasName;
	QString spoutName;
	bool autoStart = false;
};

class win_spout_config {
public:
	win_spout_config();
	static win_spout_config *get();
	void load();
	void save() const;

	bool auto_start;
	QString spout_output_name;

	QList<SpoutOutputConfig> outputs;

private:
	static win_spout_config *_instance;
};
