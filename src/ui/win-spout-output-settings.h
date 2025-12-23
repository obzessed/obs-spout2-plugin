/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#ifndef WINSPOUTOUTSETTINGS_H
#define WINSPOUTOUTSETTINGS_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

class win_spout_output_settings : public QDialog {
	Q_OBJECT

public:
	explicit win_spout_output_settings(QWidget *parent = 0);
	~win_spout_output_settings();
	void set_started_button_state(bool started) const;
	void toggle_show_hide();

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
	void on_start() const;
	void on_stop() const;
	void add_canvas();
	void remove_canvas() const;
	void on_start_selected() const;
	void on_stop_selected();
	void on_delete_selected();

private:
	void save_settings() const;
	void setupUi();
	void setupLegacyUi();
	void setupMultiCanvasUi();
	void update_row_ui(int row, bool active) const;
	void updateBulkButtonState() const;
	bool isCanvasInUse(const QString &canvasName, int excludeRow = -1) const;
	void refreshAllCanvasComboboxes() const;
	bool isSenderNameInUse(const QString &senderName, int excludeRow = -1) const;

	// Legacy Widgets
	QLineEdit *lineEdit_spoutname;
	QCheckBox *checkBox_auto;
	QPushButton *pushButton_start;
	QPushButton *pushButton_stop;

	// Multi-Canvas Widgets
	QTableWidget *tableWidget;
	QPushButton *btnAddCanvas;
	QPushButton *btnRemoveCanvas;
	QPushButton *btnStartSelected;
	QPushButton *btnStopSelected;
	QPushButton *btnDeleteSelected;
};
#endif // WINSPOUTOUTSETTINGS_H
