/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include "win-spout-output-settings.h"
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include "../win-spout-config.h"
#include "../win-spout.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSet>
#include <QStyle>
#include <QStandardItemModel>
#include <QShowEvent>
#include <QCloseEvent>
#include <QHideEvent>

win_spout_output_settings::win_spout_output_settings(QWidget *parent)
	: QDialog(parent),
	  lineEdit_spoutname(nullptr),
	  checkBox_auto(nullptr),
	  pushButton_start(nullptr),
	  pushButton_stop(nullptr),
	  tableWidget(nullptr),
	  btnAddCanvas(nullptr),
	  btnRemoveCanvas(nullptr),
	  btnStartSelected(nullptr),
	  btnStopSelected(nullptr),
	  btnDeleteSelected(nullptr)
{
	setupUi();

	win_spout_config *config = win_spout_config::get();

#if SUPPORTS_MULTI_CANVAS
	// Populate Table
	if (tableWidget) {
		for (const auto &conf : config->outputs) {
			int row = tableWidget->rowCount();
			tableWidget->insertRow(row);

			auto *canvasCombo = new QComboBox();
			canvasCombo->setEditable(true);
			canvasCombo->setFocusPolicy(Qt::NoFocus);
			canvasCombo->setStyleSheet(
				"QComboBox { padding: 4px 6px; background-color: #3b3b3b; border: 1px solid #555; border-radius: 4px; } QComboBox::drop-down { border: none; }");

			// Wrap in container for vertical centering
			auto *comboContainer = new QWidget();
			auto *comboLayout = new QHBoxLayout(comboContainer);
			comboLayout->setContentsMargins(0, 0, 0, 0);
			comboLayout->addWidget(canvasCombo);

			// Add saved canvas name as item and set as current
			// showEvent will refresh the full list when dialog opens
			if (!conf.canvasName.isEmpty()) {
				canvasCombo->addItem(conf.canvasName);
				canvasCombo->setCurrentIndex(0);
			}

			tableWidget->setCellWidget(row, 0, comboContainer);

			// Connect combobox change to refresh all comboboxes
			connect(canvasCombo, &QComboBox::currentTextChanged, this,
				[this]() { refreshAllCanvasComboboxes(); });

			tableWidget->setItem(row, 1, new QTableWidgetItem(conf.spoutName));

			// AutoStart Toggle Switch
			auto *checkWidget = new QWidget();
			auto *checkLayout = new QHBoxLayout(checkWidget);
			checkLayout->setContentsMargins(0, 0, 0, 0);
			checkLayout->setAlignment(Qt::AlignCenter);
			auto *checkBox = new QCheckBox();
			checkBox->setChecked(conf.autoStart);
			checkBox->setFocusPolicy(Qt::NoFocus);
			checkBox->setStyleSheet(
				"QCheckBox { spacing: 0px; }"
				"QCheckBox::indicator { width: 36px; height: 20px; }"
				"QCheckBox::indicator:unchecked { "
				"  image: url(:/spout/assets/icons/toggle-off.svg); "
				"}"
				"QCheckBox::indicator:checked { "
				"  image: url(:/spout/assets/icons/toggle-on.svg); "
				"}");
			checkLayout->addWidget(checkBox);
			tableWidget->setCellWidget(row, 2, checkWidget);

			auto *actionBtn = new QPushButton("Start");
			actionBtn->setFocusPolicy(Qt::NoFocus); // Prevent hover selection

			// Use a specific property or just check backend to determine initial state?
			// Checking backend is safer.
			const bool isActive = spout_output_is_active(conf.canvasName.toUtf8().constData());

			// We set widget first, then update UI
			// But we need to connect signal first?
			// Actually let's just create button, connect, setWidget, then update_row_ui.

			connect(actionBtn, &QPushButton::clicked, this, [this, actionBtn]() {
				if (tableWidget) {
					// Find current row of this button dynamically
					int currentRow = -1;
					for (int r = 0; r < tableWidget->rowCount(); ++r) {
						QWidget *cellW = tableWidget->cellWidget(r, 3);
						if (cellW && cellW->findChild<QPushButton *>() == actionBtn) {
							currentRow = r;
							break;
						}
					}
					if (currentRow < 0)
						return;

					QString canvas = "";
					QWidget *w = tableWidget->cellWidget(currentRow, 0);
					QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
					if (!cb)
						cb = qobject_cast<QComboBox *>(w);
					if (cb)
						canvas = cb->currentText();

					QString spout = tableWidget->item(currentRow, 1)->text();

					// Validate canvas uniqueness
					if (isCanvasInUse(canvas, currentRow)) {
						QMessageBox::warning(
							this, "Duplicate Canvas",
							QString("Canvas '%1' is already assigned to another output.")
								.arg(canvas));
						return;
					}

					// Validate sender name uniqueness
					if (isSenderNameInUse(spout, currentRow)) {
						QMessageBox::warning(
							this, "Duplicate Sender Name",
							QString("Sender name '%1' is already used by another output.")
								.arg(spout));
						return;
					}

					if (spout_output_is_active(canvas.toUtf8().constData())) {
						QMessageBox::StandardButton reply = QMessageBox::question(
							this, "Stop Output",
							"Are you sure you want to stop this Spout output?",
							QMessageBox::Yes | QMessageBox::No);
						if (reply == QMessageBox::Yes) {
							spout_output_stop(canvas.toUtf8().constData());
						} else {
							return; // User cancelled
						}
					} else {
						spout_output_start(canvas.toUtf8().constData(),
								   spout.toUtf8().constData());
					}

					// Re-check state after action to ensure UI matches reality
					bool newActive = spout_output_is_active(canvas.toUtf8().constData());
					update_row_ui(currentRow, newActive);
				}
			});

			// Wrap action button in container for vertical centering
			auto *actionContainer = new QWidget();
			auto *actionLayout = new QHBoxLayout(actionContainer);
			actionLayout->setContentsMargins(0, 0, 0, 0);
			actionLayout->addWidget(actionBtn);
			tableWidget->setCellWidget(row, 3, actionContainer);
			update_row_ui(row, isActive); // Initialize visual state

			// Delete Button
			auto *delBtn = new QPushButton();
			delBtn->setIcon(QIcon(":/spout/assets/icons/trash-2.svg"));
			delBtn->setFocusPolicy(Qt::NoFocus);
			delBtn->setStyleSheet(
				"QPushButton { color: #888; background: transparent; border: none; } QPushButton:hover { color: #ff5555; }");
			delBtn->setToolTip("Remove this output");

			// Wrap delete button in container for vertical centering
			auto *delContainer = new QWidget();
			auto *delLayout = new QHBoxLayout(delContainer);
			delLayout->setContentsMargins(0, 0, 0, 0);
			delLayout->setAlignment(Qt::AlignCenter);
			delLayout->addWidget(delBtn);

			connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
				if (tableWidget) {
					// Find the row by looking for the container that holds this button
					for (int r = 0; r < tableWidget->rowCount(); ++r) {
						QWidget *cellW = tableWidget->cellWidget(r, 4);
						if (cellW && cellW->findChild<QPushButton *>() == delBtn) {
							// Found it - get canvas name
							QWidget *w = tableWidget->cellWidget(r, 0);
							QString canvas = "";
							QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
							if (!cb)
								cb = qobject_cast<QComboBox *>(w);
							if (cb)
								canvas = cb->currentText();

							// Stop if active
							if (!canvas.isEmpty() &&
							    spout_output_is_active(canvas.toUtf8().constData())) {
								spout_output_stop(canvas.toUtf8().constData());
							}

							tableWidget->removeRow(r);
							updateBulkButtonState();
							break;
						}
					}
				}
			});
			tableWidget->setCellWidget(row, 4, delContainer);
		}
	}

	// Don't auto-create outputs here - OBS hasn't fully loaded canvases yet
	// This will be handled in showEvent when the dialog is opened

	updateBulkButtonState();
#else
	// Legacy Population
	if (checkBox_auto)
		checkBox_auto->setChecked(config->auto_start);
	if (lineEdit_spoutname)
		lineEdit_spoutname->setText(config->spout_output_name);

	set_started_button_state(true);
#endif
}

void win_spout_output_settings::setupUi()
{
	setWindowTitle("Spout: Output Settings");
	resize(455, 227);

#if SUPPORTS_MULTI_CANVAS
	setupMultiCanvasUi();
#else
	setupLegacyUi();
#endif
}

void win_spout_output_settings::setupLegacyUi()
{
	auto *mainLayout = new QVBoxLayout(this);

	auto *contentWidget = new QWidget(this);
	auto *contentLayout = new QVBoxLayout(contentWidget);

	checkBox_auto = new QCheckBox("AutoStart", this);
	contentLayout->addWidget(checkBox_auto);

	auto *nameLayout = new QHBoxLayout();
	auto *label = new QLabel("Spout Output Name", this);
	lineEdit_spoutname = new QLineEdit(this);
	lineEdit_spoutname->setPlaceholderText("OBS_Spout");

	nameLayout->addWidget(label);
	nameLayout->addWidget(lineEdit_spoutname);
	contentLayout->addLayout(nameLayout);

	mainLayout->addWidget(contentWidget);

	auto *btnLayout = new QHBoxLayout();
	pushButton_start = new QPushButton("Start", this);
	pushButton_stop = new QPushButton("Stop", this);

	btnLayout->addStretch();
	btnLayout->addWidget(pushButton_start);
	btnLayout->addWidget(pushButton_stop);

	mainLayout->addLayout(btnLayout);

	connect(pushButton_start, &QPushButton::clicked, this, &win_spout_output_settings::on_start);
	connect(pushButton_stop, &QPushButton::clicked, this, &win_spout_output_settings::on_stop);
}

void win_spout_output_settings::setupMultiCanvasUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	resize(700, 350); // Wider for better layout

	tableWidget = new QTableWidget(this);
	tableWidget->setColumnCount(5); // Canvas, Spout Name, AutoStart, Status/Action, Delete
	tableWidget->setHorizontalHeaderLabels({"Canvas Source", "Spout Sender Name", "AutoStart", "Status", ""});
	tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	tableWidget->setColumnWidth(3, 80);
	tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
	tableWidget->setColumnWidth(4, 30);

	tableWidget->verticalHeader()->setVisible(false);	  // Hide row numbers
	tableWidget->verticalHeader()->setDefaultSectionSize(40); // Taller rows for better UX
	tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); // Allow multi-select
	tableWidget->setAlternatingRowColors(true);
	tableWidget->setShowGrid(false);
	tableWidget->setMouseTracking(false);				// Ensure hover doesn't trigger selection
	tableWidget->setFocusPolicy(Qt::ClickFocus);			// Only focus on click
	tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked); // Allow double-click to edit sender name

	// Apply stylesheet - removed hover styling to avoid confusion with selection
	tableWidget->setStyleSheet(
		"QTableWidget { border: none; background-color: #2b2b2b; color: #e0e0e0; outline: none; }"
		"QHeaderView::section { background-color: #3b3b3b; padding: 4px; border: none; font-weight: bold; }"
		"QTableWidget::item { padding: 5px; outline: none; }"
		"QTableWidget::item:selected { background-color: #4a4a4a; }"
		"QTableWidget::item:focus { outline: none; border: none; }");

	mainLayout->addWidget(tableWidget);

	auto *btnLayout = new QHBoxLayout();
	btnAddCanvas = new QPushButton("Add New Output", this);
	btnAddCanvas->setIcon(QIcon(":/spout/assets/icons/plus.svg"));

	// Bulk Actions
	btnStartSelected = new QPushButton("Start All", this);
	btnStartSelected->setIcon(QIcon(":/spout/assets/icons/play-circle.svg"));
	btnStopSelected = new QPushButton("Stop All", this);
	btnStopSelected->setIcon(QIcon(":/spout/assets/icons/stop-circle.svg"));
	btnDeleteSelected = new QPushButton("Remove All", this);
	btnDeleteSelected->setIcon(QIcon(":/spout/assets/icons/trash.svg"));

	// Initial call
	updateBulkButtonState();

	// Selection Change Logic
	connect(tableWidget, &QTableWidget::itemSelectionChanged, this,
		&win_spout_output_settings::updateBulkButtonState);

	// Unified button styling
	QString baseStyle = "QPushButton { border-radius: 4px; padding: 6px 14px; font-weight: 500; }";
	QString addStyle =
		baseStyle +
		" QPushButton { background-color: #4a5568; color: #e2e8f0; } QPushButton:hover { background-color: #5a6578; color: white; } QPushButton:disabled { background-color: #333; color: #555; }";
	QString startStyle =
		baseStyle +
		" QPushButton { background-color: #2e7d32; color: white; } QPushButton:hover { background-color: #1b5e20; } QPushButton:disabled { background-color: #3a3a3a; color: #666; }";
	QString stopStyle =
		baseStyle +
		" QPushButton { background-color: #c62828; color: white; } QPushButton:hover { background-color: #b71c1c; } QPushButton:disabled { background-color: #3a3a3a; color: #666; }";
	QString deleteStyle =
		baseStyle +
		" QPushButton { background-color: #363636; color: #999; } QPushButton:hover { background-color: #444; color: #ccc; } QPushButton:disabled { background-color: #2a2a2a; color: #555; }";

	// Set icon sizes
	QSize iconSize(14, 14);
	btnAddCanvas->setIconSize(iconSize);
	btnStartSelected->setIconSize(iconSize);
	btnStopSelected->setIconSize(iconSize);
	btnDeleteSelected->setIconSize(iconSize);

	btnAddCanvas->setStyleSheet(addStyle);
	btnStartSelected->setStyleSheet(startStyle);
	btnStopSelected->setStyleSheet(stopStyle);
	btnDeleteSelected->setStyleSheet(deleteStyle);

	connect(btnAddCanvas, &QPushButton::clicked, this, [this]() {
		add_canvas();
		updateBulkButtonState(); // Update "All" state availability
	});
	connect(btnStartSelected, &QPushButton::clicked, this, &win_spout_output_settings::on_start_selected);
	connect(btnStopSelected, &QPushButton::clicked, this, &win_spout_output_settings::on_stop_selected);
	connect(btnDeleteSelected, &QPushButton::clicked, this, [this]() {
		on_delete_selected();
		updateBulkButtonState(); // Refresh after bulk delete
	});

	btnLayout->addWidget(btnAddCanvas);
	btnLayout->addStretch();
	btnLayout->addSpacing(8);
	btnLayout->addWidget(btnStartSelected);
	btnLayout->addSpacing(6);
	btnLayout->addWidget(btnStopSelected);
	btnLayout->addSpacing(6);
	btnLayout->addWidget(btnDeleteSelected);

	mainLayout->addLayout(btnLayout);
}

void win_spout_output_settings::save_settings() const
{
	win_spout_config *config = win_spout_config::get();

#if SUPPORTS_MULTI_CANVAS
	if (tableWidget) {
		config->outputs.clear();
		for (int i = 0; i < tableWidget->rowCount(); ++i) {
			SpoutOutputConfig item;

			QWidget *w = tableWidget->cellWidget(i, 0);
			QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
			if (!cb)
				cb = qobject_cast<QComboBox *>(w);
			if (cb) {
				item.canvasName = cb->currentText();
			} else {
				// Fallback if cell widget missing (shouldn't happen)
				if (auto *t = tableWidget->item(i, 0))
					item.canvasName = t->text();
			}

			if (auto *sb = tableWidget->item(i, 1))
				item.spoutName = sb->text();

			// Checkbox retrieval
			QWidget *cw = tableWidget->cellWidget(i, 2);
			if (cw) {
				auto *cb = cw->findChild<QCheckBox *>();
				if (cb)
					item.autoStart = cb->isChecked();
			} else {
				if (auto *ck = tableWidget->item(i, 2))
					item.autoStart = (ck->checkState() == Qt::Checked);
			}

			config->outputs.append(item);
		}
	}
#else
	if (checkBox_auto)
		config->auto_start = checkBox_auto->isChecked();
	if (lineEdit_spoutname)
		config->spout_output_name = lineEdit_spoutname->text();
#endif

	win_spout_config::get()->save();
}

win_spout_output_settings::~win_spout_output_settings()
{
	save_settings();
}

void win_spout_output_settings::closeEvent(QCloseEvent *event)
{
	save_settings();
	QDialog::closeEvent(event);
}

void win_spout_output_settings::hideEvent(QHideEvent *event)
{
	save_settings();
	QDialog::hideEvent(event);
}

void win_spout_output_settings::toggle_show_hide()
{
	if (!isVisible())
		setVisible(true);
	else
		setVisible(false);
}

void win_spout_output_settings::on_start() const
{
#if !SUPPORTS_MULTI_CANVAS
	QByteArray spout_output_name = lineEdit_spoutname->text().toUtf8();
	set_started_button_state(false);
	save_settings();
	spout_output_start(spout_output_name);
#endif
}

void win_spout_output_settings::on_stop() const
{
#if !SUPPORTS_MULTI_CANVAS
	set_started_button_state(true);
	spout_output_stop();
#endif
}

void win_spout_output_settings::set_started_button_state(const bool started) const
{
	if (pushButton_start)
		pushButton_start->setEnabled(started);
	if (pushButton_stop)
		pushButton_stop->setEnabled(!started);
}

void win_spout_output_settings::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);

#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;

	// Get the current list of canvases from OBS
	const std::vector<std::string> currentCanvases = get_canvas_names();

	// Update each row's combobox with the latest canvas list
	for (int r = 0; r < tableWidget->rowCount(); ++r) {
		QWidget *w = tableWidget->cellWidget(r, 0);
		QComboBox *combo = w ? w->findChild<QComboBox *>() : nullptr;
		if (!combo)
			combo = qobject_cast<QComboBox *>(w); // Fallback
		if (!combo)
			continue;

		// Remember current selection
		QString currentSelection = combo->currentText();

		// Block signals to prevent triggering refresh during update
		combo->blockSignals(true);

		// Clear and repopulate with current canvases
		combo->clear();
		for (const auto &name : currentCanvases) {
			combo->addItem(QString::fromStdString(name));
		}

		// If saved selection isn't in the list, add it (for backward compatibility)
		if (!currentSelection.isEmpty() && combo->findText(currentSelection) == -1) {
			combo->addItem(currentSelection);
		}

		// Restore selection
		int idx = combo->findText(currentSelection);
		if (idx >= 0) {
			combo->setCurrentIndex(idx);
		} else if (!currentSelection.isEmpty()) {
			combo->setCurrentText(currentSelection);
		}

		combo->blockSignals(false);
	}

	// Auto-create outputs for any canvases that don't have one yet
	for (const auto &canvasName : currentCanvases) {
		QString qCanvasName = QString::fromStdString(canvasName);
		if (!isCanvasInUse(qCanvasName, -1)) {
			// This canvas has no output, create one
			add_canvas();
			// The new row is at the end
			int newRow = tableWidget->rowCount() - 1;
			// Set the canvas name
			QWidget *w = tableWidget->cellWidget(newRow, 0);
			if (QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr) {
				cb->blockSignals(true);
				cb->setCurrentText(qCanvasName);
				cb->blockSignals(false);
			}
			// Update sender name to be unique
			QString senderName = "OBS_" + qCanvasName;
			tableWidget->setItem(newRow, 1, new QTableWidgetItem(senderName));
		}
	}

	// Refresh disabled states and button states
	refreshAllCanvasComboboxes();
	updateBulkButtonState();

	// Update UI to reflect actual running state for all rows
	for (int r = 0; r < tableWidget->rowCount(); ++r) {
		QWidget *w = tableWidget->cellWidget(r, 0);
		QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
		if (cb) {
			bool active = spout_output_is_active(cb->currentText().toUtf8().constData());
			update_row_ui(r, active);
		}
	}
#endif
}

void win_spout_output_settings::add_canvas()
{
#if SUPPORTS_MULTI_CANVAS
	const int row = tableWidget->rowCount();
	tableWidget->insertRow(row);

	auto *canvasCombo = new QComboBox();
	canvasCombo->setEditable(true);
	canvasCombo->setFocusPolicy(Qt::NoFocus);
	canvasCombo->setStyleSheet(
		"QComboBox { padding: 4px 6px; background-color: #3b3b3b; border: 1px solid #555; border-radius: 4px; } QComboBox::drop-down { border: none; }");

	// Wrap in container for vertical centering
	auto *comboContainer = new QWidget();
	auto *comboLayout = new QHBoxLayout(comboContainer);
	comboLayout->setContentsMargins(0, 0, 0, 0);
	comboLayout->addWidget(canvasCombo);

	const std::vector<std::string> canvases = get_canvas_names();

	// Find first available canvas not already in use
	QString defaultCanvas = "";
	for (const auto &name : canvases) {
		QString qName = QString::fromStdString(name);
		canvasCombo->addItem(qName);
		if (defaultCanvas.isEmpty() && !isCanvasInUse(qName, row)) {
			defaultCanvas = qName;
		}
	}

	// Disable already-used canvases in dropdown
	if (auto *model = qobject_cast<QStandardItemModel *>(canvasCombo->model())) {
		for (int i = 0; i < canvasCombo->count(); ++i) {
			QString itemText = canvasCombo->itemText(i);
			if (isCanvasInUse(itemText, row)) {
				QStandardItem *item = model->item(i);
				if (item) {
					item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
				}
			}
		}
	}

	// If all canvases are in use, warn the user
	if (defaultCanvas.isEmpty() && !canvases.empty()) {
		QMessageBox::warning(this, "No Available Canvas",
				     "All available canvases are already assigned to outputs.\n\n"
				     "You can manually enter a custom canvas name.");
		defaultCanvas = "";
	} else if (!defaultCanvas.isEmpty()) {
		canvasCombo->setCurrentText(defaultCanvas);
	}

	tableWidget->setCellWidget(row, 0, comboContainer);

	connect(canvasCombo, &QComboBox::currentTextChanged, this, [this]() { refreshAllCanvasComboboxes(); });

	tableWidget->setItem(row, 1, new QTableWidgetItem("OBS_Spout_" + QString::number(row)));

	// AutoStart Toggle Switch
	auto *checkWidget = new QWidget();
	auto *checkLayout = new QHBoxLayout(checkWidget);
	checkLayout->setContentsMargins(0, 0, 0, 0);
	checkLayout->setAlignment(Qt::AlignCenter);
	auto *checkBox = new QCheckBox();
	checkBox->setFocusPolicy(Qt::NoFocus);
	checkBox->setStyleSheet("QCheckBox { spacing: 0px; }"
				"QCheckBox::indicator { width: 36px; height: 20px; }"
				"QCheckBox::indicator:unchecked { "
				"  image: url(:/spout/assets/icons/toggle-off.svg); "
				"}"
				"QCheckBox::indicator:checked { "
				"  image: url(:/spout/assets/icons/toggle-on.svg); "
				"}");
	checkLayout->addWidget(checkBox);
	tableWidget->setCellWidget(row, 2, checkWidget);

	auto *actionBtn = new QPushButton("Start");
	actionBtn->setFocusPolicy(Qt::NoFocus);

	connect(actionBtn, &QPushButton::clicked, this, [this, actionBtn]() {
		if (tableWidget) {
			// Find current row of this button
			int currentRow = -1;
			for (int r = 0; r < tableWidget->rowCount(); ++r) {
				QWidget *cellW = tableWidget->cellWidget(r, 3);
				if (cellW && cellW->findChild<QPushButton *>() == actionBtn) {
					currentRow = r;
					break;
				}
			}
			if (currentRow < 0)
				return;

			QString canvas = "";
			QWidget *w = tableWidget->cellWidget(currentRow, 0);
			if (QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr) {
				canvas = cb->currentText();
			}

			// Validate canvas uniqueness before starting
			if (isCanvasInUse(canvas, currentRow)) {
				QMessageBox::warning(this, "Duplicate Canvas",
						     QString("Canvas '%1' is already assigned to another output.\n\n"
							     "Please choose a different canvas before starting.")
							     .arg(canvas));
				return;
			}

			QString spout = tableWidget->item(currentRow, 1)->text();

			// Validate sender name uniqueness before starting
			if (isSenderNameInUse(spout, currentRow)) {
				QMessageBox::warning(this, "Duplicate Sender Name",
						     QString("Sender name '%1' is already used by another output.\n\n"
							     "Please enter a unique sender name.")
							     .arg(spout));
				return;
			}

			if (spout_output_is_active(canvas.toUtf8().constData())) {
				QMessageBox::StandardButton reply = QMessageBox::question(
					this, "Stop Output", "Are you sure you want to stop this Spout output?",
					QMessageBox::Yes | QMessageBox::No);
				if (reply == QMessageBox::Yes) {
					spout_output_stop(canvas.toUtf8().constData());
				} else {
					return;
				}
			} else {
				spout_output_start(canvas.toUtf8().constData(), spout.toUtf8().constData());
			}

			const bool newActive = spout_output_is_active(canvas.toUtf8().constData());
			update_row_ui(currentRow, newActive);
		}
	});

	// Wrap action button in container for vertical centering
	auto *actionContainer = new QWidget();
	auto *actionLayout = new QHBoxLayout(actionContainer);
	actionLayout->setContentsMargins(0, 0, 0, 0);
	actionLayout->addWidget(actionBtn);
	tableWidget->setCellWidget(row, 3, actionContainer);
	update_row_ui(row, false); // Initialize style

	// Delete Button
	auto *delBtn = new QPushButton();
	delBtn->setIcon(QIcon(":/spout/assets/icons/trash-2.svg"));
	delBtn->setFocusPolicy(Qt::NoFocus);
	delBtn->setStyleSheet(
		"QPushButton { color: #888; background: transparent; border: none; } QPushButton:hover { color: #ff5555; }");
	delBtn->setToolTip("Remove this output");

	// Wrap delete button in container for vertical centering
	auto *delContainer = new QWidget();
	auto *delLayout = new QHBoxLayout(delContainer);
	delLayout->setContentsMargins(0, 0, 0, 0);
	delLayout->setAlignment(Qt::AlignCenter);
	delLayout->addWidget(delBtn);

	connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
		if (tableWidget) {
			for (int r = 0; r < tableWidget->rowCount(); ++r) {
				QWidget *cellW = tableWidget->cellWidget(r, 4);
				if (cellW && cellW->findChild<QPushButton *>() == delBtn) {
					QWidget *w = tableWidget->cellWidget(r, 0);
					QString canvas = "";
					if (QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr)
						canvas = cb->currentText();
					if (!canvas.isEmpty()) {
						if (spout_output_is_active(canvas.toUtf8().constData())) {
							const QMessageBox::StandardButton reply = QMessageBox::question(
								this, "Remove Active Output",
								"This output is currently active. Removing it will stop the Spout stream.\n\nAre you sure?",
								QMessageBox::Yes | QMessageBox::No);
							if (reply != QMessageBox::Yes)
								return;

							spout_output_stop(canvas.toUtf8().constData());
						}
					}
					tableWidget->removeRow(r);
					updateBulkButtonState();
					refreshAllCanvasComboboxes();
					break;
				}
			}
		}
	});
	tableWidget->setCellWidget(row, 4, delContainer);
#endif
}

bool win_spout_output_settings::isCanvasInUse(const QString &canvasName, int excludeRow) const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget || canvasName.isEmpty())
		return false;

	for (int r = 0; r < tableWidget->rowCount(); ++r) {
		if (r == excludeRow)
			continue;

		QWidget *w = tableWidget->cellWidget(r, 0);
		const QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
		if (!cb)
			cb = qobject_cast<QComboBox *>(w); // Fallback
		if (cb && cb->currentText() == canvasName) {
			return true;
		}
	}
#endif
	return false;
}

bool win_spout_output_settings::isSenderNameInUse(const QString &senderName, int excludeRow) const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget || senderName.isEmpty())
		return false;

	for (int r = 0; r < tableWidget->rowCount(); ++r) {
		if (r == excludeRow)
			continue;

		QTableWidgetItem *item = tableWidget->item(r, 1);
		if (item && item->text() == senderName) {
			return true;
		}
	}
#endif
	return false;
}

void win_spout_output_settings::remove_canvas() const
{
#if SUPPORTS_MULTI_CANVAS
	int row = tableWidget->currentRow();
	if (row >= 0) {
		tableWidget->removeRow(row);
	}
#endif
}

void win_spout_output_settings::update_row_ui(int row, bool active) const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;
	if (QWidget *cellW = tableWidget->cellWidget(row, 3)) {
		// Button may be wrapped in a container
		auto *btn = cellW->findChild<QPushButton *>();
		if (!btn)
			btn = qobject_cast<QPushButton *>(cellW); // Fallback for direct button

		if (btn) {
			if (active) {
				btn->setText("STOP");
				btn->setStyleSheet(
					"QPushButton { background-color: #d32f2f; color: white; font-weight: bold; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background-color: #b71c1c; }");
			} else {
				btn->setText("START");
				btn->setStyleSheet(
					"QPushButton { background-color: #388e3c; color: white; font-weight: bold; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background-color: #2e7d32; }");
			}
		}
	}

	// Make sender name editable only when stopped
	if (QTableWidgetItem *senderItem = tableWidget->item(row, 1)) {
		if (active) {
			// Running: disable editing
			senderItem->setFlags(senderItem->flags() & ~Qt::ItemIsEditable);
		} else {
			// Stopped: enable editing
			senderItem->setFlags(senderItem->flags() | Qt::ItemIsEditable);
		}
	}

	// Disable canvas combobox when output is active
	if (QWidget *canvasWidget = tableWidget->cellWidget(row, 0)) {
		auto *canvasCombo = canvasWidget->findChild<QComboBox *>();
		if (!canvasCombo)
			canvasCombo = qobject_cast<QComboBox *>(canvasWidget);
		if (canvasCombo) {
			canvasCombo->setEnabled(!active);
		}
	}
#endif
}

void win_spout_output_settings::updateBulkButtonState() const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;
	int rowCount = tableWidget->rowCount();
	// Count rows with selection
	QSet<int> selectedRows;
	for (auto *item : tableWidget->selectedItems())
		selectedRows.insert(item->row());
	int selSize = selectedRows.size();

	bool hasRows = rowCount > 0;

	if (btnStartSelected)
		btnStartSelected->setEnabled(hasRows);
	if (btnStopSelected)
		btnStopSelected->setEnabled(hasRows);
	if (btnDeleteSelected)
		btnDeleteSelected->setEnabled(hasRows);

	// Labels logic
	if (selSize == 0) {
		if (btnStartSelected)
			btnStartSelected->setText("Start All");
		if (btnStopSelected)
			btnStopSelected->setText("Stop All");
		if (btnDeleteSelected)
			btnDeleteSelected->setText("Remove All");
	} else {
		if (btnStartSelected)
			btnStartSelected->setText(QString("Start (%1)").arg(selSize));
		if (btnStopSelected)
			btnStopSelected->setText(QString("Stop (%1)").arg(selSize));
		if (btnDeleteSelected)
			btnDeleteSelected->setText(QString("Remove (%1)").arg(selSize));
	}

	// Check if Add button should be disabled (all canvases used)
	if (btnAddCanvas) {
		std::vector<std::string> canvases = get_canvas_names();
		bool hasAvailableCanvas = false;
		for (const auto &name : canvases) {
			if (!isCanvasInUse(QString::fromStdString(name), -1)) {
				hasAvailableCanvas = true;
				break;
			}
		}
		btnAddCanvas->setEnabled(hasAvailableCanvas || canvases.empty());
	}
#endif
}

void win_spout_output_settings::refreshAllCanvasComboboxes() const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;

	// Iterate all rows and update each combobox's disabled items
	for (int r = 0; r < tableWidget->rowCount(); ++r) {
		QWidget *w = tableWidget->cellWidget(r, 0);
		QComboBox *combo = w ? w->findChild<QComboBox *>() : nullptr;
		if (!combo)
			combo = qobject_cast<QComboBox *>(w); // Fallback
		if (!combo)
			continue;

		auto *model = qobject_cast<QStandardItemModel *>(combo->model());
		if (!model)
			continue;

		QString currentSelection = combo->currentText();

		for (int i = 0; i < combo->count(); ++i) {
			QString itemText = combo->itemText(i);
			QStandardItem *item = model->item(i);
			if (!item)
				continue;

			// Enable if this is the current selection or if not in use by other rows
			bool isCurrentSelection = (itemText == currentSelection);
			bool inUseByOther = isCanvasInUse(itemText, r);

			if (isCurrentSelection || !inUseByOther) {
				item->setFlags(item->flags() | Qt::ItemIsEnabled);
			} else {
				item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
			}
		}
	}
#endif
}

void win_spout_output_settings::on_start_selected() const
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;
	QList<QTableWidgetItem *> selected = tableWidget->selectedItems();
	QSet<int> rows;

	if (selected.isEmpty()) {
		// Select All behavior
		for (int i = 0; i < tableWidget->rowCount(); ++i)
			rows.insert(i);
	} else {
		for (auto *item : selected)
			rows.insert(item->row());
	}

	for (int row : rows) {
		// Retrieve info
		QString canvas = "";
		QWidget *w = tableWidget->cellWidget(row, 0);
		QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
		if (!cb)
			cb = qobject_cast<QComboBox *>(w);
		if (cb)
			canvas = cb->currentText();
		QString spout = tableWidget->item(row, 1)->text();

		if (canvas.isEmpty())
			continue;

		// Skip if duplicates (don't show warning in bulk mode, just skip)
		if (isCanvasInUse(canvas, row))
			continue;
		if (isSenderNameInUse(spout, row))
			continue;

		// Always try to start if not active.
		// Even if UI says "START" but backend is confused, this will align them.
		if (!spout_output_is_active(canvas.toUtf8().constData())) {
			spout_output_start(canvas.toUtf8().constData(), spout.toUtf8().constData());
		}

		// Always update UI to match REALITY, regardless of what we just did.
		bool isActive = spout_output_is_active(canvas.toUtf8().constData());
		update_row_ui(row, isActive);
	}
#endif
}

void win_spout_output_settings::on_stop_selected()
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;
	QList<QTableWidgetItem *> selected = tableWidget->selectedItems();
	QSet<int> rows;

	if (selected.isEmpty()) {
		// Select All behavior
		for (int i = 0; i < tableWidget->rowCount(); ++i)
			rows.insert(i);
	} else {
		for (auto *item : selected)
			rows.insert(item->row());
	}

	if (rows.isEmpty())
		return;

	// Check if any need stopping
	bool anyActive = false;
	for (int row : rows) {
		QString canvas = "";
		if (QWidget *w = tableWidget->cellWidget(row, 0)) {
			auto *cb = w->findChild<QComboBox *>();
			if (!cb)
				cb = qobject_cast<QComboBox *>(w);
			if (cb)
				canvas = cb->currentText();
		}
		if (spout_output_is_active(canvas.toUtf8().constData())) {
			anyActive = true;
			break;
		}
	}

	if (anyActive) {
		QMessageBox::StandardButton reply =
			QMessageBox::question(this, "Stop Selected Outputs",
					      "Are you sure you want to stop the selected active Spout outputs?",
					      QMessageBox::Yes | QMessageBox::No);
		if (reply != QMessageBox::Yes)
			return;
	}

	for (int row : rows) {
		QString canvas = "";
		QWidget *w = tableWidget->cellWidget(row, 0);
		QComboBox *cb = w ? w->findChild<QComboBox *>() : nullptr;
		if (!cb)
			cb = qobject_cast<QComboBox *>(w);
		if (cb)
			canvas = cb->currentText();

		if (canvas.isEmpty())
			continue;

		// Stop if active
		if (spout_output_is_active(canvas.toUtf8().constData())) {
			spout_output_stop(canvas.toUtf8().constData());
		}

		// Update UI to match REALITY
		bool isActive = spout_output_is_active(canvas.toUtf8().constData());
		update_row_ui(row, isActive);
	}
#endif
}

void win_spout_output_settings::on_delete_selected()
{
#if SUPPORTS_MULTI_CANVAS
	if (!tableWidget)
		return;
	QList<QTableWidgetItem *> selected = tableWidget->selectedItems();
	QSet<int> rows;

	if (selected.isEmpty()) {
		// Select All behavior
		for (int i = 0; i < tableWidget->rowCount(); ++i)
			rows.insert(i);
	} else {
		for (auto *item : selected)
			rows.insert(item->row());
	}

	if (rows.isEmpty())
		return;

	// Check for active ones
	bool anyActive = false;
	for (int row : rows) {
		QString canvas = "";
		if (QWidget *w = tableWidget->cellWidget(row, 0)) {
			auto *cb = w->findChild<QComboBox *>();
			if (!cb)
				cb = qobject_cast<QComboBox *>(w);
			if (cb)
				canvas = cb->currentText();
		}
		if (spout_output_is_active(canvas.toUtf8().constData())) {
			anyActive = true;
			break;
		}
	}

	QString msg = "Are you sure you want to remove the selected outputs?";
	if (anyActive) {
		msg += "\n\nWARNING: Some selected outputs are currently active and will be stopped.";
	}

	QMessageBox::StandardButton reply =
		QMessageBox::question(this, "Remove Selected", msg, QMessageBox::Yes | QMessageBox::No);
	if (reply != QMessageBox::Yes)
		return;

	// Convert to list to sort descending (to delete from bottom up)
	QList<int> sortedRows = rows.values();
	std::sort(sortedRows.begin(), sortedRows.end(), std::greater<>());

	for (int row : sortedRows) {
		QString canvas = "";
		if (QWidget *w = tableWidget->cellWidget(row, 0)) {
			auto *cb = w->findChild<QComboBox *>();
			if (!cb)
				cb = qobject_cast<QComboBox *>(w);
			if (cb)
				canvas = cb->currentText();
		}

		if (!canvas.isEmpty() && spout_output_is_active(canvas.toUtf8().constData())) {
			spout_output_stop(canvas.toUtf8().constData());
		}
		tableWidget->removeRow(row);
	}

	updateBulkButtonState(); // Refresh "All" availability
#endif
}
