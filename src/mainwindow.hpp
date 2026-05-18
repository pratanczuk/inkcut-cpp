// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QByteArray>
#include <QDateTime>
#include <QMainWindow>
#include <QMap>
#include <QPointF>
#include <QStringList>

#include "app_settings.hpp"
#include "device_presets.hpp"
#include "filters.hpp"
#include "job_history.hpp"
#include "job_model.hpp"
#include "live_plot_view.hpp"
#include "preview_plot_view.hpp"
#include "ui_icons.hpp"

class QCheckBox;
class QComboBox;
class QDockWidget;
class QWidget;
class QDoubleSpinBox;
class QGridLayout;
class QGraphicsItemGroup;
class QGraphicsPathItem;
class QGraphicsScene;
class QTimer;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QSerialPort;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QThread;

namespace inkcut {

class DevicePluginLoader;
class PlotSendWorker;

class DevicePlugin;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void applyStartupSettings(const AppSettings& settings);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onOpenSvg();
    void onImportBitmap();
    void onExportProgram();
    void onExportJobJson();
    void onSend();
    void onRecentFileTriggered();
    void onOpenDeviceSetup();
    void onOpenSettings();
    void onLayerOrColorFilterChanged();

    void onMonitorClear();
    void onLiveListenToggled(bool on);

    void onControlConnectToggle();
    void onControlPenUp();
    void onControlPenDown();
    void onControlSetOrigin();
    void onControlGoOrigin();
    void onControlGoSystemOrigin();
    void onControlMoveUp();
    void onControlMoveDown();
    void onControlMoveLeft();
    void onControlMoveRight();
    void onControlMaterialLoad();
    void onControlMaterialUnload();
    void updateControlStatusLabel();
    bool sendControlMove(double x, double y, double z);
    bool sendControlRawCommand(const QString& command);
    bool sendDeviceCommandBlock(const QString& commands);
    void runDeviceConnectCommands();

    void refreshDevicePlugins();
    void onPluginComboChanged(int idx);
    void onPresetChanged(int idx);
    void onTransportChanged(int idx);
    void onSendPauseResume();
    void onSendCancel();
    void onLiveActionClicked();
    void onLiveClearPlot();
    void onSendProgress(qint64 sent, qint64 total);
    void onSendLivePosition(double x, double y);
    void onGrblSettingsReady(const QMap<int, double>& settings);
    void onSendFinished(bool ok, const QString& err);

    void onLayoutChanged();
    void onPreviewGraphicOffsetChanged(qreal x, qreal y);
    void onPreviewGraphicDragFinished();
    void onAddCopyStack();
    void onRemoveCopyStack();
    void refreshLayerAndColorLists();
    void addPassFilterListRow(QListWidget* list, const QString& storage_key, const QString& label,
                              bool enabled, int pass_count, int row_height);
    void updateGraphicSizeLabels();
    void updateMaterialCutParamsVisibility();

private:
    void rebuildPreview();
    void rebuildRecentMenu();
    void pushRecentPath(const QString& path);
    bool applyOpenedDesign(const QString& path);
    bool loadCurrentDesign(QPainterPath& out, QString& err, QStringList* warns = nullptr);

    PlotJobSettings collectJobSettings() const;
    void applyJobSettingsToUi(const PlotJobSettings& job);
    void applyAppSettingsToUi();
    void applyUiProfile();
    void updateLayersPanelMetrics();
    void applyPresetToUi(const DevicePreset& preset);
    void loadPersistedSettings();
    void savePersistedSettings();
    bool confirmSendApproval(const PlotJobSettings& job, const QPainterPath& model_path);

    void appendMonitorRaw(const QByteArray& data, bool is_tx);
    void ingestRxPlotterHints(const QByteArray& rx);
    void drainSerialToMonitor(QSerialPort& serial);
    void stopLiveListen();
    void stopControlConnection();
    void applyLiveActionUi(UiIcons::LiveAction action);
    void applyControlConnectUi(bool connected);
    void stopSendWorker();

    void recordJobHistory(JobRunStatus status, qint64 bytes_sent, const QString& err);
    void rebuildJobHistoryUi();
    void openJobHistoryRow(int row);
    void removeJobHistoryRow(int row);
    void showJobHistoryContextMenu(const QPoint& pos);
    void showSvgWarningsIfAny(const QStringList& warnings);
    void updateFilePathLabel();
    void rebuildLivePlotScene();
    void updateLiveStatusBar(qint64 sent, qint64 total);
    void updateLiveStatusTick();
    void showLivePlotContextMenu(const QPoint& pos);
    void onLiveFitAll();
    void onConsoleCommand();

    DevicePlugin* activePlugin() const;
    void updateUnitSuffixes();
    void retranslateUi();

    QGraphicsScene* scene_ = nullptr;
    QGraphicsScene* live_scene_ = nullptr;
    LivePlotView* live_view_ = nullptr;
    QGraphicsPathItem* live_trail_item_ = nullptr;
    QGraphicsPathItem* live_move_path_item_ = nullptr;
    QGraphicsPathItem* live_job_path_item_ = nullptr;
    QGraphicsPathItem* live_material_item_ = nullptr;
    QString current_file_;
    QString current_svg_xml_;
    bool design_is_filled_trace_ = false;

    QWidget* device_host_ = nullptr;
    QComboBox* order_combo_ = nullptr;
    QComboBox* preset_combo_ = nullptr;
    QComboBox* transport_combo_ = nullptr;
    QComboBox* plugin_combo_ = nullptr;
    QLineEdit* port_edit_ = nullptr;
    QSpinBox* baud_spin_ = nullptr;
    QMenu* file_menu_ = nullptr;
    QMenu* device_menu_ = nullptr;
    QMenu* settings_menu_ = nullptr;
    QMenu* help_menu_ = nullptr;
    QMenu* recent_menu_ = nullptr;
    QAction* open_svg_action_ = nullptr;
    QAction* import_bitmap_action_ = nullptr;

    QDoubleSpinBox* mat_w_spin_ = nullptr;
    QDoubleSpinBox* mat_h_spin_ = nullptr;
    QDoubleSpinBox* mat_pad_l_spin_ = nullptr;
    QDoubleSpinBox* mat_pad_t_spin_ = nullptr;
    QDoubleSpinBox* mat_pad_r_spin_ = nullptr;
    QDoubleSpinBox* mat_pad_b_spin_ = nullptr;
    QCheckBox* auto_shift_chk_ = nullptr;
    QCheckBox* align_center_x_chk_ = nullptr;
    QCheckBox* align_center_y_chk_ = nullptr;
        QRadioButton* feed_return_rb_ = nullptr;
    QRadioButton* feed_after_rb_ = nullptr;
    QDoubleSpinBox* feed_after_spin_ = nullptr;

    QLabel* graphic_size_w_label_ = nullptr;
    QLabel* graphic_size_h_label_ = nullptr;
    QDoubleSpinBox* scale_pct_x_spin_ = nullptr;
    QDoubleSpinBox* scale_pct_y_spin_ = nullptr;
    QDoubleSpinBox* layout_offset_x_spin_ = nullptr;
    QDoubleSpinBox* layout_offset_y_spin_ = nullptr;
    QDoubleSpinBox* scale_x_spin_ = nullptr;
    QDoubleSpinBox* scale_y_spin_ = nullptr;
    QDoubleSpinBox* rotation_spin_ = nullptr;
    QSpinBox* copies_spin_ = nullptr;
    QDoubleSpinBox* copy_gap_x_spin_ = nullptr;
    QDoubleSpinBox* copy_gap_y_spin_ = nullptr;
    QCheckBox* mirror_x_chk_ = nullptr;
    QCheckBox* mirror_y_chk_ = nullptr;
    QCheckBox* auto_rotate_chk_ = nullptr;
    QCheckBox* auto_copies_chk_ = nullptr;
    QCheckBox* auto_scale_chk_ = nullptr;
    QCheckBox* lock_scale_chk_ = nullptr;
    QCheckBox* mat_roll_chk_ = nullptr;
    QCheckBox* mat_force_speed_chk_ = nullptr;
    QWidget* mat_cutter_cut_params_ = nullptr;
    QSpinBox* mat_force_spin_ = nullptr;
    QSpinBox* mat_speed_spin_ = nullptr;
    QWidget* mat_gcode_cut_params_ = nullptr;
    QSpinBox* mat_gcode_feed_spin_ = nullptr;
    QSpinBox* mat_gcode_feed_rapid_spin_ = nullptr;
    QPushButton* add_stack_btn_ = nullptr;
    QPushButton* remove_stack_btn_ = nullptr;

    QCheckBox* plot_weedline_chk_ = nullptr;
    QCheckBox* copy_weedline_chk_ = nullptr;
    QDoubleSpinBox* copy_pad_l_spin_ = nullptr;
    QDoubleSpinBox* copy_pad_t_spin_ = nullptr;
    QDoubleSpinBox* copy_pad_r_spin_ = nullptr;
    QDoubleSpinBox* copy_pad_b_spin_ = nullptr;
    QDoubleSpinBox* plot_pad_l_spin_ = nullptr;
    QDoubleSpinBox* plot_pad_t_spin_ = nullptr;
    QDoubleSpinBox* plot_pad_r_spin_ = nullptr;
    QDoubleSpinBox* plot_pad_b_spin_ = nullptr;

    QListWidget* layer_list_ = nullptr;
    QLabel* dxf_layers_hint_ = nullptr;
    QListWidget* fill_color_list_ = nullptr;
    QListWidget* stroke_color_list_ = nullptr;
    QScrollArea* layers_scroll_ = nullptr;

    QPushButton* plugin_refresh_btn_ = nullptr;
    QPushButton* send_pause_btn_ = nullptr;
    QPushButton* send_cancel_btn_ = nullptr;
    QLabel* live_source_label_ = nullptr;
    QLabel* live_size_label_ = nullptr;
    QLabel* live_duration_label_ = nullptr;
    QLabel* live_left_label_ = nullptr;
    QLabel* live_eta_label_ = nullptr;
    QProgressBar* live_progress_bar_ = nullptr;
    QPushButton* live_abort_btn_ = nullptr;
    QPushButton* live_action_btn_ = nullptr;

    std::unique_ptr<DevicePluginLoader> plugin_loader_;

    QPlainTextEdit* console_output_ = nullptr;
    QLineEdit* console_input_ = nullptr;
    QPlainTextEdit* monitor_edit_ = nullptr;
    QCheckBox* monitor_log_send_chk_ = nullptr;
    QCheckBox* monitor_hex_chk_ = nullptr;
    QCheckBox* monitor_live_chk_ = nullptr;
    QLabel* plot_position_label_ = nullptr;
    QByteArray rx_accum_;

    QTabWidget* left_tabs_ = nullptr;
    QTabWidget* bottom_tabs_ = nullptr;
    QTableWidget* history_table_ = nullptr;
    QString active_history_id_;
    QLabel* file_path_label_ = nullptr;

    std::unique_ptr<QSerialPort> live_serial_;

    QSpinBox* control_step_spin_ = nullptr;
    QLabel* control_status_label_ = nullptr;
    QPushButton* control_connect_btn_ = nullptr;
    QPushButton* control_load_btn_ = nullptr;
    QPushButton* control_unload_btn_ = nullptr;
    QGridLayout* control_grid_ = nullptr;
    std::unique_ptr<QSerialPort> control_serial_;
    QPointF control_pos_;
    QPointF control_origin_;
    double control_z_ = 0;

    std::unique_ptr<QThread> send_thread_;
    PlotSendWorker* send_worker_ = nullptr;
    bool send_paused_ = false;
    bool send_active_ = false;
    qint64 send_total_bytes_ = 0;
    qint64 send_progress_sent_ = 0;
    QDateTime send_started_;
    QDateTime send_estimated_end_;
    int send_duration_estimate_sec_ = 0;
    QTimer* live_status_timer_ = nullptr;

    PlotJobSettings last_job_settings_;

    PlotJobSettings pipeline_cache_;
    AppSettings app_settings_;
    DeviceSetup active_device_;
    PreviewPlotView* preview_view_ = nullptr;
    QGraphicsItemGroup* preview_graphic_group_ = nullptr;
    QDockWidget* bottom_dock_ = nullptr;
    QWidget* control_tab_ = nullptr;
    QPlainTextEdit* grbl_diag_edit_ = nullptr;
    QMap<int, double> grbl_settings_cache_ui_;
};

} // namespace inkcut
