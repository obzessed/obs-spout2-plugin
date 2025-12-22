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

class win_spout_config {
public:
	win_spout_config();
	static win_spout_config *get();
	void load();
	void save() const;

	bool auto_start;
	QString spout_output_name;

private:
	static win_spout_config *_instance;
};
