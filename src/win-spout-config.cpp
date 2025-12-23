/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "win-spout-config.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#define SECTION_NAME "win_spout"
#define PARAM_AUTO_START "auto_start"
#define PARAM_SPOUT_OUTPUT_NAME "spout_output_name"
#define PARAM_OUTPUTS_LIST "outputs_list"

win_spout_config *win_spout_config::_instance = nullptr;

win_spout_config::win_spout_config() : auto_start(false), spout_output_name("OBS_Spout")
{
	if (config_t *obs_config = obs_frontend_get_user_config()) {
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_AUTO_START, auto_start);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME,
					  spout_output_name.toUtf8().constData());
		config_set_default_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST, "[]");
	}
}

void win_spout_config::load()
{
	if (config_t *obs_config = obs_frontend_get_user_config()) {
		auto_start = config_get_bool(obs_config, SECTION_NAME, PARAM_AUTO_START);
		spout_output_name = config_get_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME);

		const char *json_str = config_get_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST);
		if (json_str && *json_str) {
			outputs.clear();
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json_str));
			QJsonArray arr = doc.array();
			for (const auto &val : arr) {
				QJsonObject obj = val.toObject();
				SpoutOutputConfig conf;
				conf.canvasName = obj["canvasName"].toString();
				conf.spoutName = obj["spoutName"].toString();
				conf.autoStart = obj["autoStart"].toBool();
				outputs.append(conf);
			}
		}
	}
}

void win_spout_config::save() const
{
	if (config_t *obs_config = obs_frontend_get_user_config()) {
		config_set_bool(obs_config, SECTION_NAME, PARAM_AUTO_START, auto_start);
		config_set_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME,
				  spout_output_name.toUtf8().constData());

		QJsonArray arr;
		for (const auto &conf : outputs) {
			QJsonObject obj;
			obj["canvasName"] = conf.canvasName;
			obj["spoutName"] = conf.spoutName;
			obj["autoStart"] = conf.autoStart;
			arr.append(obj);
		}
		QJsonDocument doc(arr);
		config_set_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST,
				  doc.toJson(QJsonDocument::Compact).constData());

		config_save(obs_config);
	}
}

win_spout_config *win_spout_config::get()
{
	if (!_instance) {
		_instance = new win_spout_config();
	}
	return _instance;
}
