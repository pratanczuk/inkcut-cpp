// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_setup_dialog.hpp"

#include "app_settings.hpp"
#include "device_plugin.hpp"
#include "device_presets.hpp"
#include "filters.hpp"
#include "i18n.hpp"
#include "plugin_loader.hpp"
#include "ui_icons.hpp"
#include "ui_profile.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

#include <QSerialPortInfo>

#include <algorithm>

namespace inkcut {

namespace {

void styleFormLayout(QFormLayout* form, UiProfile profile)
{
    form->setVerticalSpacing(profile == UiProfile::Tablet ? 12 : 8);
    form->setHorizontalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

/// QFormLayout::setRowVisible wymaga Qt >= 6.8; jammy ma Qt 6.2.
void setFormRowVisible(QFormLayout* form, QWidget* field, bool visible)
{
    if (!form || !field)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    form->setRowVisible(field, visible);
#else
    field->setVisible(visible);
    if (QWidget* label = form->labelForField(field))
        label->setVisible(visible);
#endif
}

QScrollArea* wrapTabInScroll(QWidget* page)
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    return scroll;
}

} // namespace

namespace {

struct DeviceProfile {
    QString name;
    bool custom = false;
    DeviceSetup device;
    double width = 600;
    double height = 400;
    /// Physical work area of the plotter (mm). 0 = unknown.
    double work_area_width = 0.0;
    double work_area_height = 0.0;
    PlotProtocol protocol = PlotProtocol::GCode;
    int velocity = 120;
    bool gcode_builtin = true;
    GCodeProtocolSettings::Dialect gcode_dialect = GCodeProtocolSettings::Dialect::Generic;
    GCodeProtocolSettings::LiftMode gcode_lift = GCodeProtocolSettings::Implicit;
    int gcode_precision = 3;
    double gcode_upper_z = 5.0;
    double gcode_lower_z = 0.0;
    int gcode_pwm_up = 0;
    int gcode_pwm_down = 700;
    int gcode_pwm_max = 1000;
    int gcode_feed_mm_min = 0;
    int gcode_feed_rapid_mm_min = 0;
    BladeOffsetConfig blade;
    double overcut = 0;
    double closed_poly_eps = 0.25;
    RepeatFilterConfig repeat;
    MinLineFilterConfig min_line;
    QString gcode_lift_gcode;
    QString gcode_lower_gcode;
    QString plugin_id;
};

QString protocolLabel(PlotProtocol p)
{
    Q_UNUSED(p);
    return QStringLiteral("G-code");
}

void fillPresetCombo(QComboBox* combo)
{
    combo->clear();
    for (const DevicePreset& p : devicePresets())
        combo->addItem(QStringLiteral("%1 %2").arg(p.manufacturer, p.model), p.id);
}

void fillPluginCombo(QComboBox* combo, DevicePluginLoader* loader)
{
    combo->clear();
    combo->addItem(trInk("(brak)"), QString());
    if (!loader)
        return;
    for (DevicePlugin* p : loader->plugins())
        combo->addItem(p->displayName(), p->pluginId());
}

int presetIndex(QComboBox* combo, const QString& id)
{
    const int i = combo->findData(id);
    return i >= 0 ? i : 0;
}

DeviceProfile profileFromJob(const PlotJobSettings& job)
{
    DeviceProfile p;
    p.device = job.device;
    p.custom = job.device.custom;
    p.width = job.material.width;
    p.height = job.material.height;
    p.work_area_width = job.device.work_area_width;
    p.work_area_height = job.device.work_area_height;
    p.velocity = job.material.speed > 0 ? job.material.speed : job.velocity;
    p.gcode_feed_mm_min = job.material.gcode_feed_cut_mm_min > 0
                              ? job.material.gcode_feed_cut_mm_min
                              : job.protocol.gcode.feed_mm_min;
    p.gcode_feed_rapid_mm_min = job.material.gcode_feed_rapid_mm_min > 0
                                    ? job.material.gcode_feed_rapid_mm_min
                                    : job.protocol.gcode.feed_rapid_mm_min;
    p.protocol = job.protocol.protocol;
    p.velocity = job.velocity;
    p.gcode_builtin = job.protocol.gcode.use_builtin;
    p.gcode_dialect = job.protocol.gcode.dialect;
    p.gcode_lift = job.protocol.gcode.lift_mode;
    p.gcode_precision = job.protocol.gcode.precision;
    p.gcode_upper_z = job.protocol.gcode.upper_z;
    p.gcode_lower_z = job.protocol.gcode.lower_z;
    p.gcode_pwm_up = job.protocol.gcode.solenoid_pwm_up;
    p.gcode_pwm_down = job.protocol.gcode.solenoid_pwm_down;
    p.gcode_pwm_max = job.protocol.gcode.solenoid_pwm_max;
    p.gcode_feed_mm_min = job.protocol.gcode.feed_mm_min;
    p.gcode_feed_rapid_mm_min = job.protocol.gcode.feed_rapid_mm_min;
    p.blade = job.blade;
    p.overcut = job.overcut;
    p.repeat = job.repeat;
    p.min_line = job.min_line;
    p.closed_poly_eps = job.closed_poly_eps;
    p.gcode_lift_gcode = job.protocol.gcode.lift_gcode;
    p.gcode_lower_gcode = job.protocol.gcode.lower_gcode;
    p.plugin_id = job.plugin_id;
    if (!job.device.name.isEmpty())
        p.name = job.device.name;
    else if (!job.device.manufacturer.isEmpty())
        p.name = QStringLiteral("%1 %2").arg(job.device.manufacturer, job.device.model_name);
    else
        p.name = trInk("Nowe urządzenie");
    return p;
}

void profileToJob(const DeviceProfile& p, PlotJobSettings& job)
{
    job.device = p.device;
    job.device.custom = p.custom;
    job.device.name = p.name;
    job.device.work_area_width = p.work_area_width;
    job.device.work_area_height = p.work_area_height;
    // Material size is edited only in the main window Material tab — not from device profile.
    job.material.speed = p.velocity;
    job.material.gcode_feed_cut_mm_min = p.gcode_feed_mm_min;
    job.material.gcode_feed_rapid_mm_min = p.gcode_feed_rapid_mm_min;
    job.velocity = p.velocity;
    job.protocol.protocol = p.protocol;
    job.velocity = p.velocity;
    job.protocol.gcode.use_builtin = p.gcode_builtin;
    job.protocol.gcode.dialect = p.gcode_dialect;
    job.protocol.gcode.lift_mode = p.gcode_lift;
    job.protocol.gcode.precision = p.gcode_precision;
    job.protocol.gcode.upper_z = p.gcode_upper_z;
    job.protocol.gcode.lower_z = p.gcode_lower_z;
    job.protocol.gcode.solenoid_pwm_up = p.gcode_pwm_up;
    job.protocol.gcode.solenoid_pwm_down = p.gcode_pwm_down;
    job.protocol.gcode.solenoid_pwm_max = p.gcode_pwm_max;
    job.protocol.gcode.feed_mm_min = p.gcode_feed_mm_min;
    job.protocol.gcode.feed_rapid_mm_min = p.gcode_feed_rapid_mm_min;
    job.blade = p.blade;
    job.overcut = p.overcut;
    job.repeat = p.repeat;
    job.min_line = p.min_line;
    job.closed_poly_eps = p.closed_poly_eps;
    job.protocol.gcode.lift_gcode = p.gcode_lift_gcode;
    job.protocol.gcode.lower_gcode = p.gcode_lower_gcode;
    job.plugin_id = p.plugin_id;
}

void applyPresetToProfile(const DevicePreset& preset, DeviceProfile& p)
{
    p.device.preset_id = preset.id;
    p.device.manufacturer = preset.manufacturer;
    p.device.model_name = preset.model;
    p.device.baud_rate = preset.default_baud;
    p.device.swap_xy = preset.swap_xy;
    p.device.mirror_x = preset.mirror_x;
    p.device.mirror_y = preset.mirror_y;
    p.protocol = preset.default_protocol;
    if (preset.default_protocol == PlotProtocol::GCode) {
        p.gcode_dialect = preset.gcode_dialect;
        p.gcode_lift = preset.gcode_lift_mode;
        p.gcode_pwm_up = preset.solenoid_pwm_up;
        p.gcode_pwm_down = preset.solenoid_pwm_down;
        p.gcode_pwm_max = preset.solenoid_pwm_max;
    }
    p.work_area_width = preset.work_area_width;
    p.work_area_height = preset.work_area_height;
    if (p.name.isEmpty() || p.name == trInk("Nowe urządzenie"))
        p.name = QStringLiteral("%1 %2").arg(preset.manufacturer, preset.model);
}

QJsonObject profileToJson(const DeviceProfile& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), p.name);
    o.insert(QStringLiteral("custom"), p.custom);
    o.insert(QStringLiteral("preset_id"), p.device.preset_id);
    o.insert(QStringLiteral("manufacturer"), p.device.manufacturer);
    o.insert(QStringLiteral("model_name"), p.device.model_name);
    o.insert(QStringLiteral("transport"), int(p.device.transport));
    o.insert(QStringLiteral("port"), p.device.port_name);
    o.insert(QStringLiteral("baud"), int(p.device.baud_rate));
    o.insert(QStringLiteral("tcp_host"), p.device.tcp_host);
    o.insert(QStringLiteral("tcp_port"), p.device.tcp_port);
    o.insert(QStringLiteral("data_bits"), p.device.data_bits);
    o.insert(QStringLiteral("parity"), p.device.parity);
    o.insert(QStringLiteral("stop_bits"), p.device.stop_bits);
    o.insert(QStringLiteral("flow_rts_cts"), p.device.flow_rts_cts);
    o.insert(QStringLiteral("flow_dsr_dtr"), p.device.flow_dsr_dtr);
    o.insert(QStringLiteral("flow_xon_xoff"), p.device.flow_xon_xoff);
    o.insert(QStringLiteral("swap_xy"), p.device.swap_xy);
    o.insert(QStringLiteral("mirror_x"), p.device.mirror_x);
    o.insert(QStringLiteral("mirror_y"), p.device.mirror_y);
    o.insert(QStringLiteral("device_scale"), p.device.device_scale);
    o.insert(QStringLiteral("width"), p.width);
    o.insert(QStringLiteral("height"), p.height);
    o.insert(QStringLiteral("work_area_width"), p.work_area_width);
    o.insert(QStringLiteral("work_area_height"), p.work_area_height);
    o.insert(QStringLiteral("protocol"), int(p.protocol));
    o.insert(QStringLiteral("velocity"), p.velocity);
    o.insert(QStringLiteral("gcode_builtin"), p.gcode_builtin);
    o.insert(QStringLiteral("gcode_dialect"), int(p.gcode_dialect));
    o.insert(QStringLiteral("gcode_lift"), int(p.gcode_lift));
    o.insert(QStringLiteral("gcode_precision"), p.gcode_precision);
    o.insert(QStringLiteral("gcode_upper_z"), p.gcode_upper_z);
    o.insert(QStringLiteral("gcode_lower_z"), p.gcode_lower_z);
    o.insert(QStringLiteral("gcode_pwm_up"), p.gcode_pwm_up);
    o.insert(QStringLiteral("gcode_pwm_down"), p.gcode_pwm_down);
    o.insert(QStringLiteral("gcode_pwm_max"), p.gcode_pwm_max);
    o.insert(QStringLiteral("gcode_feed_mm_min"), p.gcode_feed_mm_min);
    o.insert(QStringLiteral("gcode_feed_rapid_mm_min"), p.gcode_feed_rapid_mm_min);
    QJsonObject blade;
    blade.insert(QStringLiteral("offset"), p.blade.offset);
    blade.insert(QStringLiteral("cutoff_deg"), p.blade.cutoff_deg);
    blade.insert(QStringLiteral("quality_factor"), p.blade.quality_factor);
    o.insert(QStringLiteral("blade"), blade);
    o.insert(QStringLiteral("overcut"), p.overcut);
    o.insert(QStringLiteral("closed_poly_eps"), p.closed_poly_eps);
    o.insert(QStringLiteral("gcode_lift_gcode"), p.gcode_lift_gcode);
    o.insert(QStringLiteral("gcode_lower_gcode"), p.gcode_lower_gcode);
    o.insert(QStringLiteral("plugin_id"), p.plugin_id);
    o.insert(QStringLiteral("before_connect"), p.device.before_connect_command);
    o.insert(QStringLiteral("after_connect"), p.device.after_connect_command);
    o.insert(QStringLiteral("before_job"), p.device.before_job_command);
    o.insert(QStringLiteral("after_job"), p.device.after_job_command);
    QJsonObject repeat;
    repeat.insert(QStringLiteral("steps"), p.repeat.steps);
    repeat.insert(QStringLiteral("closed_loop_distance"), p.repeat.closed_loop_distance);
    o.insert(QStringLiteral("repeat"), repeat);
    QJsonObject min_line;
    min_line.insert(QStringLiteral("min_jump"), p.min_line.min_jump);
    min_line.insert(QStringLiteral("min_path"), p.min_line.min_path);
    min_line.insert(QStringLiteral("min_edge"), p.min_line.min_edge);
    min_line.insert(QStringLiteral("min_shift"), p.min_line.min_shift);
    o.insert(QStringLiteral("min_line"), min_line);
    return o;
}

DeviceProfile profileFromJson(const QJsonObject& o)
{
    DeviceProfile p;
    p.name = o.value(QStringLiteral("name")).toString(trInk("Nowe urządzenie"));
    p.custom = o.value(QStringLiteral("custom")).toBool();
    p.device.preset_id = o.value(QStringLiteral("preset_id")).toString();
    p.device.manufacturer = o.value(QStringLiteral("manufacturer")).toString();
    p.device.model_name = o.value(QStringLiteral("model_name")).toString();
    p.device.transport =
        static_cast<PlotTransportKind>(o.value(QStringLiteral("transport")).toInt());
    p.device.port_name = o.value(QStringLiteral("port")).toString();
    p.device.baud_rate = o.value(QStringLiteral("baud")).toInt(115200);
    p.device.tcp_host = o.value(QStringLiteral("tcp_host")).toString(p.device.tcp_host);
    p.device.tcp_port = o.value(QStringLiteral("tcp_port")).toInt(p.device.tcp_port);
    p.device.data_bits = o.value(QStringLiteral("data_bits")).toInt(8);
    p.device.parity = o.value(QStringLiteral("parity")).toInt(0);
    p.device.stop_bits = o.value(QStringLiteral("stop_bits")).toInt(1);
    p.device.flow_rts_cts = o.value(QStringLiteral("flow_rts_cts")).toBool();
    p.device.flow_dsr_dtr = o.value(QStringLiteral("flow_dsr_dtr")).toBool();
    p.device.flow_xon_xoff = o.value(QStringLiteral("flow_xon_xoff")).toBool();
    p.device.swap_xy = o.value(QStringLiteral("swap_xy")).toBool();
    p.device.mirror_x = o.value(QStringLiteral("mirror_x")).toBool();
    p.device.mirror_y = o.value(QStringLiteral("mirror_y")).toBool();
    p.device.device_scale = o.value(QStringLiteral("device_scale")).toDouble(1.0);
    p.width = o.value(QStringLiteral("width")).toDouble(600);
    p.height = o.value(QStringLiteral("height")).toDouble(400);
    p.work_area_width = o.value(QStringLiteral("work_area_width")).toDouble(300.0);
    p.work_area_height = o.value(QStringLiteral("work_area_height")).toDouble(300.0);
    if (p.work_area_width <= 0.0)
        p.work_area_width = 300.0;
    if (p.work_area_height <= 0.0)
        p.work_area_height = 300.0;
    p.protocol = static_cast<PlotProtocol>(o.value(QStringLiteral("protocol")).toInt());
    p.velocity = o.value(QStringLiteral("velocity")).toInt(120);
    p.gcode_builtin = o.value(QStringLiteral("gcode_builtin")).toBool(true);
    p.gcode_dialect = static_cast<GCodeProtocolSettings::Dialect>(
        o.value(QStringLiteral("gcode_dialect")).toInt());
    p.gcode_lift = static_cast<GCodeProtocolSettings::LiftMode>(
        o.value(QStringLiteral("gcode_lift")).toInt());
    p.gcode_precision = o.value(QStringLiteral("gcode_precision")).toInt(3);
    p.gcode_upper_z = o.value(QStringLiteral("gcode_upper_z")).toDouble(5.0);
    p.gcode_lower_z = o.value(QStringLiteral("gcode_lower_z")).toDouble();
    p.gcode_pwm_up = o.value(QStringLiteral("gcode_pwm_up")).toInt(0);
    p.gcode_pwm_down = o.value(QStringLiteral("gcode_pwm_down")).toInt(700);
    p.gcode_pwm_max = o.value(QStringLiteral("gcode_pwm_max")).toInt(1000);
    p.gcode_feed_mm_min = o.value(QStringLiteral("gcode_feed_mm_min")).toInt(0);
    p.gcode_feed_rapid_mm_min = o.value(QStringLiteral("gcode_feed_rapid_mm_min")).toInt(0);
    const QJsonObject blade = o.value(QStringLiteral("blade")).toObject();
    if (!blade.isEmpty()) {
        p.blade.offset = blade.value(QStringLiteral("offset")).toDouble();
        p.blade.cutoff_deg = blade.value(QStringLiteral("cutoff_deg")).toDouble(5.0);
        p.blade.quality_factor = blade.value(QStringLiteral("quality_factor")).toDouble(1.0);
    }
    p.overcut = o.value(QStringLiteral("overcut")).toDouble();
    p.closed_poly_eps = o.value(QStringLiteral("closed_poly_eps")).toDouble(0.25);
    p.gcode_lift_gcode = o.value(QStringLiteral("gcode_lift_gcode")).toString();
    p.gcode_lower_gcode = o.value(QStringLiteral("gcode_lower_gcode")).toString();
    p.plugin_id = o.value(QStringLiteral("plugin_id")).toString();
    p.device.before_connect_command = o.value(QStringLiteral("before_connect")).toString();
    p.device.after_connect_command = o.value(QStringLiteral("after_connect")).toString();
    p.device.before_job_command = o.value(QStringLiteral("before_job")).toString();
    p.device.after_job_command = o.value(QStringLiteral("after_job")).toString();
    const QJsonObject repeat = o.value(QStringLiteral("repeat")).toObject();
    if (!repeat.isEmpty()) {
        p.repeat.steps = repeat.value(QStringLiteral("steps")).toInt(1);
        p.repeat.closed_loop_distance =
            repeat.value(QStringLiteral("closed_loop_distance")).toDouble(0.1);
    }
    const QJsonObject min_line = o.value(QStringLiteral("min_line")).toObject();
    if (!min_line.isEmpty()) {
        p.min_line.min_jump = min_line.value(QStringLiteral("min_jump")).toDouble();
        p.min_line.min_path = min_line.value(QStringLiteral("min_path")).toDouble();
        p.min_line.min_edge = min_line.value(QStringLiteral("min_edge")).toDouble();
        p.min_line.min_shift = min_line.value(QStringLiteral("min_shift")).toDouble();
    }
    if (p.device.manufacturer.isEmpty() || p.device.model_name.isEmpty()) {
        DevicePreset preset;
        if (devicePresetById(p.device.preset_id, preset)) {
            if (p.device.manufacturer.isEmpty())
                p.device.manufacturer = preset.manufacturer;
            if (p.device.model_name.isEmpty())
                p.device.model_name = preset.model;
        }
    }
    return p;
}

QVector<DeviceProfile> loadDeviceProfiles(const PlotJobSettings& fallback)
{
    QVector<DeviceProfile> out;
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    const QByteArray raw = settings.value(QStringLiteral("device_profiles_v1")).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isArray()) {
        for (const QJsonValue& v : doc.array())
            out.push_back(profileFromJson(v.toObject()));
    }
    if (out.isEmpty())
        out.push_back(profileFromJob(fallback));
    return out;
}

void saveDeviceProfiles(const QVector<DeviceProfile>& profiles, int active_index)
{
    QJsonArray arr;
    for (const DeviceProfile& p : profiles)
        arr.append(profileToJson(p));
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    settings.setValue(QStringLiteral("device_profiles_v1"),
                      QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    settings.setValue(QStringLiteral("active_device_profile_index"), active_index);
    settings.sync();
}

int loadActiveDeviceProfileIndex()
{
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    return settings.value(QStringLiteral("active_device_profile_index"), 0).toInt();
}

int findProfileIndex(const QVector<DeviceProfile>& profiles, const PlotJobSettings& job)
{
    const int saved = loadActiveDeviceProfileIndex();
    if (saved >= 0 && saved < profiles.size())
        return saved;
    for (int i = 0; i < profiles.size(); ++i) {
        if (!job.device.name.isEmpty() && profiles[i].name == job.device.name)
            return i;
    }
    for (int i = 0; i < profiles.size(); ++i) {
        if (profiles[i].device.preset_id == job.device.preset_id)
            return i;
    }
    return 0;
}

class DeviceSetupDialog final {
public:
    DeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins)
        : job_(job)
        , plugins_(plugins)
    {
        profiles_ = loadDeviceProfiles(job);
        current_ = findProfileIndex(profiles_, job);

        dlg_ = new QDialog(parent);
        dlg_->setWindowTitle(trInk("Konfiguracja urządzenia — Inkcut"));
        AppSettings app;
        loadAppSettings(app);
        ui_profile_ = app.ui_profile;
        applyDialogProfile(dlg_, metricsFor(app.ui_profile).device_setup_dialog, app.ui_profile);

        buildUi();
        loadProfileIntoUi(current_);
        updateCustomFieldsEnabled();
        updateTransportVisibility();
        wireSignals();
    }

    bool exec()
    {
        return dlg_->exec() == QDialog::Accepted;
    }

private:
    int deviceListRowHeight() const
    {
        return metricsFor(ui_profile_).device_list_row_height;
    }

    void syncDeviceListRows()
    {
        if (!device_list_)
            return;
        const int h = deviceListRowHeight();
        device_list_->setSpacing(ui_profile_ == UiProfile::Tablet ? 6 : 2);
        for (int i = 0; i < device_list_->count(); ++i) {
            if (QListWidgetItem* it = device_list_->item(i))
                it->setSizeHint(QSize(0, h));
        }
    }

    void appendDeviceListItem(const QString& name)
    {
        auto* it = new QListWidgetItem(name);
        it->setSizeHint(QSize(0, deviceListRowHeight()));
        device_list_->addItem(it);
    }

    void buildUi()
    {
        device_list_ = new QListWidget(dlg_);
        device_list_->setObjectName(QStringLiteral("device_profile_list"));
        device_list_->setMinimumWidth(ui_profile_ == UiProfile::Tablet ? 200 : 160);
        if (ui_profile_ == UiProfile::Tablet) {
            QFont f = device_list_->font();
            f.setPointSize(14);
            device_list_->setFont(f);
        }
        for (const DeviceProfile& p : profiles_)
            appendDeviceListItem(p.name);
        syncDeviceListRows();

        add_btn_ = new QPushButton(trInk("+ Dodaj"), dlg_);
        auto* left = new QVBoxLayout();
        left->addWidget(new QLabel(trInk("Dostępne urządzenia"), dlg_));
        left->addWidget(device_list_, 1);
        left->addWidget(add_btn_);

        tabs_ = new QTabWidget(dlg_);

        // --- Ogólne (2 columns) ---
        auto* general = new QWidget(tabs_);
        auto* ggrid = new QGridLayout(general);
        ggrid->setContentsMargins(6, 6, 6, 6);
        ggrid->setHorizontalSpacing(8);
        ggrid->setVerticalSpacing(6);
        name_edit_ = new QLineEdit(general);
        driver_combo_ = new QComboBox(general);
        fillPresetCombo(driver_combo_);
        mfg_edit_ = new QLineEdit(general);
        model_edit_ = new QLineEdit(general);
        custom_chk_ = new QCheckBox(trInk("Własne"), general);
        ggrid->addWidget(new QLabel(trInk("Nazwa"), general), 0, 0);
        ggrid->addWidget(name_edit_, 0, 1);
        ggrid->addWidget(new QLabel(trInk("Sterownik"), general), 0, 2);
        ggrid->addWidget(driver_combo_, 0, 3);
        ggrid->addWidget(new QLabel(trInk("Producent"), general), 1, 0);
        ggrid->addWidget(mfg_edit_, 1, 1);
        ggrid->addWidget(new QLabel(trInk("Model"), general), 1, 2);
        ggrid->addWidget(model_edit_, 1, 3);
        ggrid->addWidget(custom_chk_, 2, 0, 1, 4);
        ggrid->setColumnStretch(1, 1);
        ggrid->setColumnStretch(3, 1);
        tabs_->addTab(wrapTabInScroll(general), trInk("Ogólne"));

        // --- Urządzenie ---
        auto* device_tab = new QWidget(tabs_);
        auto* dform = new QFormLayout(device_tab);
        styleFormLayout(dform, ui_profile_);
        swap_chk_ = new QCheckBox(trInk("Zamień X/Y"), device_tab);
        mirror_x_chk_ = new QCheckBox(trInk("Lustro X"), device_tab);
        mirror_y_chk_ = new QCheckBox(trInk("Lustro Y"), device_tab);
        scale_spin_ = new QDoubleSpinBox(device_tab);
        scale_spin_->setRange(0.001, 1000);
        scale_spin_->setDecimals(6);
        work_area_w_spin_ = new QDoubleSpinBox(device_tab);
        work_area_h_spin_ = new QDoubleSpinBox(device_tab);
        for (QDoubleSpinBox* s : {work_area_w_spin_, work_area_h_spin_}) {
            s->setRange(1.0, 99999.9);
            s->setDecimals(2);
            s->setSuffix(QStringLiteral(" mm"));
        }
        dform->addRow(swap_chk_);
        dform->addRow(mirror_x_chk_);
        dform->addRow(mirror_y_chk_);
        dform->addRow(trInk("Skala wyjścia"), scale_spin_);
        auto* work_area_row = new QWidget(device_tab);
        auto* work_area_grid = new QGridLayout(work_area_row);
        work_area_grid->setContentsMargins(0, 0, 0, 0);
        work_area_grid->setHorizontalSpacing(8);
        work_area_grid->addWidget(new QLabel(trInk("Obszar roboczy — szer."), work_area_row), 0, 0);
        work_area_grid->addWidget(work_area_w_spin_, 0, 1);
        work_area_grid->addWidget(new QLabel(trInk("Obszar roboczy — wys."), work_area_row), 0, 2);
        work_area_grid->addWidget(work_area_h_spin_, 0, 3);
        work_area_grid->setColumnStretch(1, 1);
        work_area_grid->setColumnStretch(3, 1);
        dform->addRow(work_area_row);

        auto* commands_group = new QGroupBox(trInk("Komendy"), device_tab);
        auto* commands_grid = new QGridLayout(commands_group);
        commands_grid->setContentsMargins(8, 8, 8, 8);
        commands_grid->setHorizontalSpacing(12);
        commands_grid->setVerticalSpacing(10);
        const int cmd_edit_h = ui_profile_ == UiProfile::Tablet ? 88 : 72;
        auto makeCmdPanel = [&](const QString& label, QPlainTextEdit*& edit,
                                const QString& placeholder, int row, int col) {
            auto* panel = new QWidget(commands_group);
            panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            auto* pv = new QVBoxLayout(panel);
            pv->setContentsMargins(0, 0, 0, 0);
            pv->setSpacing(4);
            auto* lbl = new QLabel(label, panel);
            lbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            pv->addWidget(lbl);
            edit = new QPlainTextEdit(panel);
            edit->setMinimumHeight(cmd_edit_h);
            edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            edit->setPlaceholderText(placeholder);
            pv->addWidget(edit, 1);
            commands_grid->addWidget(panel, row, col);
        };
        makeCmdPanel(trInk("Przed połączeniem"), before_connect_edit_,
                     trInk("Przed połączeniem (np. reset\\n)"), 0, 0);
        makeCmdPanel(trInk("Po połączeniu"), after_connect_edit_,
                     trInk("Po połączeniu (np. G21\\n)"), 0, 1);
        makeCmdPanel(trInk("Przed zadaniem"), before_job_edit_, trInk("Przed cięciem"), 1, 0);
        makeCmdPanel(trInk("Po zadaniu"), after_job_edit_, trInk("Po zadaniu"), 1, 1);
        commands_grid->setColumnStretch(0, 1);
        commands_grid->setColumnStretch(1, 1);
        dform->addRow(commands_group);

        tabs_->addTab(wrapTabInScroll(device_tab), trInk("Urządzenie"));

        // --- Połączenie ---
        auto* conn = new QWidget(tabs_);
        conn_form_ = new QFormLayout(conn);
        styleFormLayout(conn_form_, ui_profile_);
        transport_combo_ = new QComboBox(conn);
        transport_combo_->addItem(trInk("Port szeregowy"), int(PlotTransportKind::SerialPort));
        transport_combo_->addItem(QStringLiteral("TCP/IP"), int(PlotTransportKind::TcpIp));

        port_row_widget_ = new QWidget(conn);
        port_combo_ = new QComboBox(port_row_widget_);
        refresh_port_btn_ = new QPushButton(port_row_widget_);
        refresh_port_btn_->setIcon(UiIcons::refresh(conn));
        refresh_port_btn_->setIconSize(QSize(22, 22));
        refresh_port_btn_->setToolTip(trInk("Odśwież"));
        auto* port_row = new QHBoxLayout(port_row_widget_);
        port_row->setContentsMargins(0, 0, 0, 0);
        port_row->addWidget(port_combo_, 1);
        port_row->addWidget(refresh_port_btn_);

        serial_params_row_widget_ = new QWidget(conn);
        auto* serial_params_row = new QHBoxLayout(serial_params_row_widget_);
        serial_params_row->setContentsMargins(0, 0, 0, 0);
        serial_params_row->setSpacing(12);

        baud_spin_ = new QSpinBox(serial_params_row_widget_);
        baud_spin_->setRange(1200, 1000000);
        baud_spin_->setValue(115200);

        bytesize_combo_ = new QComboBox(serial_params_row_widget_);
        for (int bits : {5, 6, 7, 8})
            bytesize_combo_->addItem(QString::number(bits), bits);

        parity_combo_ = new QComboBox(serial_params_row_widget_);
        parity_combo_->addItem(trInk("Brak"), 0);
        parity_combo_->addItem(trInk("Parzysta"), 1);
        parity_combo_->addItem(trInk("Nieparzysta"), 2);
        parity_combo_->addItem(QStringLiteral("Space"), 3);
        parity_combo_->addItem(QStringLiteral("Mark"), 4);

        stopbits_combo_ = new QComboBox(serial_params_row_widget_);
        stopbits_combo_->addItem(QStringLiteral("1"), 1);
        stopbits_combo_->addItem(QStringLiteral("2"), 2);

        for (QComboBox* combo : {bytesize_combo_, parity_combo_, stopbits_combo_})
            combo->setMinimumContentsLength(6);

        auto addSerialField = [&](const QString& label, QWidget* field) {
            auto* lbl = new QLabel(label, serial_params_row_widget_);
            lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            serial_params_row->addWidget(lbl);
            serial_params_row->addWidget(field);
        };
        addSerialField(trInk("Prędkość (baud)"), baud_spin_);
        addSerialField(trInk("Bity danych"), bytesize_combo_);
        addSerialField(trInk("Parzystość"), parity_combo_);
        addSerialField(trInk("Bity stopu"), stopbits_combo_);
        serial_params_row->addStretch();

        flow_widget_ = new QWidget(conn);
        auto* flow_row = new QHBoxLayout(flow_widget_);
        flow_row->setContentsMargins(0, 0, 0, 0);
        flow_rts_chk_ = new QCheckBox(QStringLiteral("RTS/CTS"), flow_widget_);
        flow_dsr_chk_ = new QCheckBox(QStringLiteral("DSR/DTR"), flow_widget_);
        flow_xon_chk_ = new QCheckBox(QStringLiteral("XON/XOFF"), flow_widget_);
        flow_row->addWidget(flow_rts_chk_);
        flow_row->addWidget(flow_dsr_chk_);
        flow_row->addWidget(flow_xon_chk_);
        flow_row->addStretch();

        tcp_row_widget_ = new QWidget(conn);
        auto* tcp_row = new QHBoxLayout(tcp_row_widget_);
        tcp_row->setContentsMargins(0, 0, 0, 0);
        tcp_host_edit_ = new QLineEdit(tcp_row_widget_);
        tcp_host_edit_->setPlaceholderText(QStringLiteral("127.0.0.1"));
        tcp_port_spin_ = new QSpinBox(tcp_row_widget_);
        tcp_port_spin_->setRange(1, 65535);
        tcp_port_spin_->setValue(23);
        tcp_row->addWidget(tcp_host_edit_, 1);
        tcp_row->addWidget(new QLabel(QStringLiteral(":"), tcp_row_widget_));
        tcp_row->addWidget(tcp_port_spin_);

        conn_form_->addRow(trInk("Typ"), transport_combo_);
        conn_form_->addRow(trInk("Port"), port_row_widget_);
        conn_form_->addRow(QString(), serial_params_row_widget_);
        conn_form_->addRow(QStringLiteral("TCP"), tcp_row_widget_);
        conn_form_->addRow(trInk("Kontrola przepływu"), flow_widget_);
        tabs_->addTab(wrapTabInScroll(conn), trInk("Połączenie"));

        // --- Protokół ---
        auto* proto = new QWidget(tabs_);
        auto* pform = new QFormLayout(proto);
        styleFormLayout(pform, ui_profile_);
        protocol_combo_ = new QComboBox(proto);
        protocol_combo_->addItem(protocolLabel(PlotProtocol::GCode), int(PlotProtocol::GCode));
        gcode_builtin_chk_ = new QCheckBox(trInk("Wbudowane komendy start/stop"), proto);
        gcode_lift_combo_ = new QComboBox(proto);
        gcode_lift_combo_->addItem(QStringLiteral("Implicit (G00/G01)"),
                                   int(GCodeProtocolSettings::Implicit));
        gcode_lift_combo_->addItem(trInk("Oś Z"),
                                   int(GCodeProtocolSettings::ZAxis));
        gcode_lift_combo_->addItem(trInk("Własne G-code"),
                                   int(GCodeProtocolSettings::Custom));
        gcode_lift_combo_->addItem(trInk("Solenoid PWM (M3 S…)"),
                                   int(GCodeProtocolSettings::SolenoidPwm));
        gcode_dialect_combo_ = new QComboBox(proto);
        gcode_dialect_combo_->addItem(trInk("Ogólny"),
                                      int(GCodeProtocolSettings::Dialect::Generic));
        gcode_dialect_combo_->addItem(QStringLiteral("GRBL"),
                                      int(GCodeProtocolSettings::Dialect::Grbl));
        gcode_upper_z_spin_ = new QDoubleSpinBox(proto);
        gcode_lower_z_spin_ = new QDoubleSpinBox(proto);
        gcode_precision_spin_ = new QSpinBox(proto);
        gcode_precision_spin_->setRange(0, 6);
        for (QDoubleSpinBox* s : {gcode_upper_z_spin_, gcode_lower_z_spin_}) {
            s->setRange(-999, 999);
            s->setDecimals(3);
        }
        gcode_group_ = new QGroupBox(trInk("G-code / GRBL"), proto);
        auto* gcode_form = new QFormLayout(gcode_group_);
        gcode_form->addRow(gcode_builtin_chk_);
        gcode_form->addRow(trInk("Dialekt"), gcode_dialect_combo_);
        gcode_form->addRow(trInk("Podnoszenie narzędzia"), gcode_lift_combo_);
        gcode_form->addRow(trInk("Precyzja"), gcode_precision_spin_);
        gcode_z_up_label_ = new QLabel(trInk("Z góra (mm)"), gcode_group_);
        gcode_z_down_label_ = new QLabel(trInk("Z dół (mm)"), gcode_group_);
        gcode_form->addRow(gcode_z_up_label_, gcode_upper_z_spin_);
        gcode_form->addRow(gcode_z_down_label_, gcode_lower_z_spin_);
        gcode_lift_edit_ = new QPlainTextEdit(gcode_group_);
        gcode_lift_edit_->setMaximumHeight(56);
        gcode_lift_edit_->setPlaceholderText(trInk("G-code podniesienia (tryb Własne)"));
        gcode_lower_edit_ = new QPlainTextEdit(gcode_group_);
        gcode_lower_edit_->setMaximumHeight(56);
        gcode_lower_edit_->setPlaceholderText(trInk("G-code opuszczenia (tryb Własne)"));
        gcode_lift_edit_label_ = new QLabel(trInk("Lift G-code"), gcode_group_);
        gcode_lower_edit_label_ = new QLabel(trInk("Lower G-code"), gcode_group_);
        gcode_form->addRow(gcode_lift_edit_label_, gcode_lift_edit_);
        gcode_form->addRow(gcode_lower_edit_label_, gcode_lower_edit_);

        pwm_up_spin_ = new QSpinBox(gcode_group_);
        pwm_up_spin_->setRange(0, 65535);
        pwm_down_spin_ = new QSpinBox(gcode_group_);
        pwm_down_spin_->setRange(0, 65535);
        pwm_max_spin_ = new QSpinBox(gcode_group_);
        pwm_max_spin_->setRange(1, 65535);
        pwm_up_label_ = new QLabel(trInk("PWM pióro w górze (M5 jeśli 0)"), gcode_group_);
        pwm_down_label_ = new QLabel(trInk("PWM nacisk (pióro w dole)"), gcode_group_);
        pwm_max_label_ = new QLabel(trInk("PWM maks. ($30 w GRBL)"), gcode_group_);
        gcode_form->addRow(pwm_up_label_, pwm_up_spin_);
        gcode_form->addRow(pwm_down_label_, pwm_down_spin_);
        gcode_form->addRow(pwm_max_label_, pwm_max_spin_);

        pform->addRow(trInk("Język"), protocol_combo_);
        pform->addRow(gcode_group_);
        tabs_->addTab(wrapTabInScroll(proto), trInk("Protokół"));

        // --- Filtry urządzenia (2-column grid so all groups fit on one screen) ---
        auto* filters = new QWidget(tabs_);
        auto* fgrid = new QGridLayout(filters);
        fgrid->setContentsMargins(6, 6, 6, 6);
        fgrid->setHorizontalSpacing(8);
        fgrid->setVerticalSpacing(8);

        auto* blade_gb = new QGroupBox(trInk("Offset ostrza"), filters);
        auto* blade_grid = new QGridLayout(blade_gb);
        blade_grid->setContentsMargins(8, 8, 8, 8);
        blade_grid->setHorizontalSpacing(6);
        blade_grid->setVerticalSpacing(4);
        blade_offset_spin_ = new QDoubleSpinBox(blade_gb);
        blade_offset_spin_->setRange(0, 50);
        blade_offset_spin_->setDecimals(3);
        blade_cutoff_spin_ = new QDoubleSpinBox(blade_gb);
        blade_cutoff_spin_->setRange(0, 90);
        blade_cutoff_spin_->setDecimals(1);
        blade_quality_spin_ = new QDoubleSpinBox(blade_gb);
        blade_quality_spin_->setRange(0.001, 100);
        blade_grid->addWidget(new QLabel(trInk("Offset"), blade_gb), 0, 0);
        blade_grid->addWidget(blade_offset_spin_, 0, 1);
        blade_grid->addWidget(new QLabel(trInk("Kąt odcięcia"), blade_gb), 0, 2);
        blade_grid->addWidget(blade_cutoff_spin_, 0, 3);
        blade_grid->addWidget(new QLabel(trInk("Jakość"), blade_gb), 1, 0);
        blade_grid->addWidget(blade_quality_spin_, 1, 1, 1, 3);
        blade_grid->setColumnStretch(1, 1);
        blade_grid->setColumnStretch(3, 1);

        auto* overcut_gb = new QGroupBox(QStringLiteral("Overcut"), filters);
        auto* overcut_grid = new QGridLayout(overcut_gb);
        overcut_grid->setContentsMargins(8, 8, 8, 8);
        overcut_grid->setHorizontalSpacing(6);
        overcut_grid->setVerticalSpacing(4);
        overcut_spin_ = new QDoubleSpinBox(overcut_gb);
        overcut_spin_->setRange(0, 50);
        overcut_spin_->setDecimals(3);
        closed_poly_eps_spin_ = new QDoubleSpinBox(overcut_gb);
        closed_poly_eps_spin_->setRange(0.001, 50);
        closed_poly_eps_spin_->setDecimals(3);
        closed_poly_eps_spin_->setValue(0.25);
        overcut_grid->addWidget(new QLabel(QStringLiteral("Overcut"), overcut_gb), 0, 0);
        overcut_grid->addWidget(overcut_spin_, 0, 1);
        overcut_grid->addWidget(new QLabel(trInk("Próg zamknięcia"), overcut_gb), 0, 2);
        overcut_grid->addWidget(closed_poly_eps_spin_, 0, 3);
        overcut_grid->setColumnStretch(1, 1);
        overcut_grid->setColumnStretch(3, 1);

        auto* min_gb = new QGroupBox(trInk("Min. linia"), filters);
        auto* min_grid = new QGridLayout(min_gb);
        min_grid->setContentsMargins(8, 8, 8, 8);
        min_grid->setHorizontalSpacing(6);
        min_grid->setVerticalSpacing(4);
        min_jump_spin_ = new QDoubleSpinBox(min_gb);
        min_path_spin_ = new QDoubleSpinBox(min_gb);
        min_shift_spin_ = new QDoubleSpinBox(min_gb);
        min_edge_spin_ = new QDoubleSpinBox(min_gb);
        for (QDoubleSpinBox* s : {min_jump_spin_, min_path_spin_, min_shift_spin_, min_edge_spin_}) {
            s->setRange(0, 1000);
            s->setDecimals(3);
        }
        min_grid->addWidget(new QLabel(trInk("Min. skok (PU)"), min_gb), 0, 0);
        min_grid->addWidget(min_jump_spin_, 0, 1);
        min_grid->addWidget(new QLabel(trInk("Min. ścieżka"), min_gb), 0, 2);
        min_grid->addWidget(min_path_spin_, 0, 3);
        min_grid->addWidget(new QLabel(trInk("Min. przesunięcie"), min_gb), 1, 0);
        min_grid->addWidget(min_shift_spin_, 1, 1);
        min_grid->addWidget(new QLabel(trInk("Min. krawędź"), min_gb), 1, 2);
        min_grid->addWidget(min_edge_spin_, 1, 3);
        min_grid->setColumnStretch(1, 1);
        min_grid->setColumnStretch(3, 1);

        auto* repeat_gb = new QGroupBox(trInk("Powtórzenia (cały job)"), filters);
        auto* repeat_grid = new QGridLayout(repeat_gb);
        repeat_grid->setContentsMargins(8, 8, 8, 8);
        repeat_grid->setHorizontalSpacing(6);
        repeat_grid->setVerticalSpacing(4);
        repeat_steps_spin_ = new QSpinBox(repeat_gb);
        repeat_steps_spin_->setRange(1, 50);
        repeat_gap_spin_ = new QDoubleSpinBox(repeat_gb);
        repeat_gap_spin_->setRange(0, 100);
        repeat_gap_spin_->setDecimals(3);
        repeat_grid->addWidget(new QLabel(trInk("Powtórzenia"), repeat_gb), 0, 0);
        repeat_grid->addWidget(repeat_steps_spin_, 0, 1);
        repeat_grid->addWidget(new QLabel(trInk("Max. luka zamknięcia"), repeat_gb), 0, 2);
        repeat_grid->addWidget(repeat_gap_spin_, 0, 3);
        repeat_grid->setColumnStretch(1, 1);
        repeat_grid->setColumnStretch(3, 1);
        auto* repeat_hint = new QLabel(
            trInk("Powtórzenia pojedynczej warstwy (np. twardy materiał) ustaw w głównym "
                  "oknie: lewy panel → zakładka Warstwy."),
            repeat_gb);
        repeat_hint->setWordWrap(true);
        repeat_grid->addWidget(repeat_hint, 1, 0, 1, 4);

        fgrid->addWidget(blade_gb, 0, 0);
        fgrid->addWidget(overcut_gb, 0, 1);
        fgrid->addWidget(min_gb, 1, 0);
        fgrid->addWidget(repeat_gb, 1, 1);
        fgrid->setColumnStretch(0, 1);
        fgrid->setColumnStretch(1, 1);
        fgrid->setRowStretch(0, 1);
        fgrid->setRowStretch(1, 1);
        tabs_->addTab(wrapTabInScroll(filters), trInk("Filtry"));

        splitter_ = new QSplitter(Qt::Horizontal, dlg_);
        auto* left_w = new QWidget(dlg_);
        left_w->setLayout(left);
        splitter_->addWidget(left_w);
        splitter_->addWidget(tabs_);
        splitter_->setStretchFactor(1, 4);

        active_lbl_ = new QLabel(dlg_);
        buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg_);
        auto* root = new QVBoxLayout(dlg_);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);
        root->addWidget(splitter_, 1);
        root->addWidget(active_lbl_);
        root->addWidget(buttons_);

        device_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    }

    void wireSignals()
    {
        QObject::connect(device_list_, &QListWidget::currentRowChanged, dlg_,
                         [this](int row) {
                             if (row < 0 || syncing_)
                                 return;
                             saveUiToProfile(current_);
                             current_ = row;
                             loadProfileIntoUi(current_);
                         });
        QObject::connect(add_btn_, &QPushButton::clicked, dlg_, [this]() { addDevice(); });
        QObject::connect(device_list_, &QListWidget::customContextMenuRequested, dlg_,
                         [this](const QPoint& pos) { showListMenu(pos); });
        QObject::connect(custom_chk_, &QCheckBox::toggled, dlg_, [this](bool) {
            updateCustomFieldsEnabled();
        });
        QObject::connect(driver_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this](int idx) {
                             if (syncing_ || custom_chk_->isChecked())
                                 return;
                             DevicePreset preset;
                             if (!devicePresetById(driver_combo_->itemData(idx).toString(), preset))
                                 return;
                             DeviceProfile& p = profiles_[current_];
                             applyPresetToProfile(preset, p);
                             loadProfileIntoUi(current_);
                             updateProtocolLockedByPreset();
                         });
        QObject::connect(transport_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateTransportVisibility(); });
        QObject::connect(refresh_port_btn_, &QPushButton::clicked, dlg_,
                         [this]() { refreshSerialPorts(); });
        QObject::connect(protocol_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateProtocolTabVisibility(); });
        QObject::connect(gcode_lift_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateProtocolTabVisibility(); });
        QObject::connect(buttons_, &QDialogButtonBox::accepted, dlg_, [this]() {
            saveUiToProfile(current_);
            saveDeviceProfiles(profiles_, current_);
            profileToJob(profiles_[current_], job_);
            dlg_->accept();
        });
        QObject::connect(buttons_, &QDialogButtonBox::rejected, dlg_, &QDialog::reject);
    }

    void loadProfileIntoUi(int index)
    {
        if (index < 0 || index >= profiles_.size())
            return;
        syncing_ = true;
        const DeviceProfile& p = profiles_[index];
        device_list_->setCurrentRow(index);
        active_lbl_->setText(trInk("Aktywne urządzenie: %1").arg(p.name));
        name_edit_->setText(p.name);
        driver_combo_->setCurrentIndex(presetIndex(driver_combo_, p.device.preset_id));
        mfg_edit_->setText(p.device.manufacturer);
        model_edit_->setText(p.device.model_name);
        custom_chk_->setChecked(p.custom);
        swap_chk_->setChecked(p.device.swap_xy);
        mirror_x_chk_->setChecked(p.device.mirror_x);
        mirror_y_chk_->setChecked(p.device.mirror_y);
        scale_spin_->setValue(p.device.device_scale);
        work_area_w_spin_->setValue(p.work_area_width > 0.0 ? p.work_area_width : 300.0);
        work_area_h_spin_->setValue(p.work_area_height > 0.0 ? p.work_area_height : 300.0);
        PlotTransportKind transport = p.device.transport;
        transport_combo_->setCurrentIndex(transport_combo_->findData(int(transport)));
        refreshSerialPorts();
        setSerialPortInCombo(p.device.port_name);
        baud_spin_->setValue(p.device.baud_rate);
        tcp_host_edit_->setText(p.device.tcp_host);
        tcp_port_spin_->setValue(p.device.tcp_port > 0 ? p.device.tcp_port : 23);
        const int bs = bytesize_combo_->findData(p.device.data_bits);
        if (bs >= 0)
            bytesize_combo_->setCurrentIndex(bs);
        const int par = parity_combo_->findData(p.device.parity);
        if (par >= 0)
            parity_combo_->setCurrentIndex(par);
        const int st = stopbits_combo_->findData(p.device.stop_bits);
        if (st >= 0)
            stopbits_combo_->setCurrentIndex(st);
        flow_rts_chk_->setChecked(p.device.flow_rts_cts);
        flow_dsr_chk_->setChecked(p.device.flow_dsr_dtr);
        flow_xon_chk_->setChecked(p.device.flow_xon_xoff);
        protocol_combo_->setCurrentIndex(protocol_combo_->findData(int(p.protocol)));
        gcode_builtin_chk_->setChecked(p.gcode_builtin);
        gcode_dialect_combo_->setCurrentIndex(
            gcode_dialect_combo_->findData(int(p.gcode_dialect)));
        gcode_lift_combo_->setCurrentIndex(gcode_lift_combo_->findData(int(p.gcode_lift)));
        gcode_precision_spin_->setValue(p.gcode_precision);
        gcode_upper_z_spin_->setValue(p.gcode_upper_z);
        gcode_lower_z_spin_->setValue(p.gcode_lower_z);
        pwm_up_spin_->setValue(p.gcode_pwm_up);
        pwm_down_spin_->setValue(p.gcode_pwm_down);
        pwm_max_spin_->setValue(p.gcode_pwm_max);
        blade_offset_spin_->setValue(p.blade.offset);
        blade_cutoff_spin_->setValue(p.blade.cutoff_deg);
        blade_quality_spin_->setValue(p.blade.quality_factor);
        overcut_spin_->setValue(p.overcut);
        closed_poly_eps_spin_->setValue(p.closed_poly_eps);
        gcode_lift_edit_->setPlainText(p.gcode_lift_gcode);
        gcode_lower_edit_->setPlainText(p.gcode_lower_gcode);
        before_connect_edit_->setPlainText(p.device.before_connect_command);
        after_connect_edit_->setPlainText(p.device.after_connect_command);
        before_job_edit_->setPlainText(p.device.before_job_command);
        after_job_edit_->setPlainText(p.device.after_job_command);
        repeat_steps_spin_->setValue(p.repeat.steps);
        repeat_gap_spin_->setValue(p.repeat.closed_loop_distance);
        min_jump_spin_->setValue(p.min_line.min_jump);
        min_path_spin_->setValue(p.min_line.min_path);
        min_edge_spin_->setValue(p.min_line.min_edge);
        min_shift_spin_->setValue(p.min_line.min_shift);
        syncing_ = false;
        updateCustomFieldsEnabled();
        updateTransportVisibility();
        updateProtocolTabVisibility();
        updateProtocolLockedByPreset();
    }

    void saveUiToProfile(int index)
    {
        if (index < 0 || index >= profiles_.size())
            return;
        DeviceProfile& p = profiles_[index];
        p.name = name_edit_->text().trimmed();
        if (p.name.isEmpty())
            p.name = trInk("Nowe urządzenie");
        p.custom = custom_chk_->isChecked();
        p.device.preset_id = driver_combo_->currentData().toString();
        p.device.manufacturer = mfg_edit_->text().trimmed();
        p.device.model_name = model_edit_->text().trimmed();
        p.device.swap_xy = swap_chk_->isChecked();
        p.device.mirror_x = mirror_x_chk_->isChecked();
        p.device.mirror_y = mirror_y_chk_->isChecked();
        p.device.device_scale = scale_spin_->value();
        p.work_area_width = work_area_w_spin_->value();
        p.work_area_height = work_area_h_spin_->value();
        p.device.work_area_width = p.work_area_width;
        p.device.work_area_height = p.work_area_height;
        p.device.transport =
            static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
        p.device.port_name = port_combo_->currentData().toString();
        if (p.device.port_name.isEmpty())
            p.device.port_name = port_combo_->currentText().trimmed();
        p.device.baud_rate = baud_spin_->value();
        p.device.tcp_host = tcp_host_edit_->text().trimmed();
        p.device.tcp_port = tcp_port_spin_->value();
        p.device.data_bits = bytesize_combo_->currentData().toInt();
        p.device.parity = parity_combo_->currentData().toInt();
        p.device.stop_bits = stopbits_combo_->currentData().toInt();
        p.device.flow_rts_cts = flow_rts_chk_->isChecked();
        p.device.flow_dsr_dtr = flow_dsr_chk_->isChecked();
        p.device.flow_xon_xoff = flow_xon_chk_->isChecked();
        p.protocol = static_cast<PlotProtocol>(protocol_combo_->currentData().toInt());
        p.gcode_builtin = gcode_builtin_chk_->isChecked();
        p.gcode_dialect = static_cast<GCodeProtocolSettings::Dialect>(
            gcode_dialect_combo_->currentData().toInt());
        p.gcode_lift = static_cast<GCodeProtocolSettings::LiftMode>(
            gcode_lift_combo_->currentData().toInt());
        p.gcode_precision = gcode_precision_spin_->value();
        p.gcode_upper_z = gcode_upper_z_spin_->value();
        p.gcode_lower_z = gcode_lower_z_spin_->value();
        p.gcode_pwm_up = pwm_up_spin_->value();
        p.gcode_pwm_down = pwm_down_spin_->value();
        p.gcode_pwm_max = pwm_max_spin_->value();
        p.velocity = job_.material.speed > 0 ? job_.material.speed : job_.velocity;
        p.gcode_feed_mm_min = job_.material.gcode_feed_cut_mm_min;
        p.gcode_feed_rapid_mm_min = job_.material.gcode_feed_rapid_mm_min;
        p.blade.offset = blade_offset_spin_->value();
        p.blade.cutoff_deg = blade_cutoff_spin_->value();
        p.blade.quality_factor = blade_quality_spin_->value();
        p.overcut = overcut_spin_->value();
        p.closed_poly_eps = closed_poly_eps_spin_->value();
        p.gcode_lift_gcode = gcode_lift_edit_->toPlainText();
        p.gcode_lower_gcode = gcode_lower_edit_->toPlainText();
        p.device.before_connect_command = before_connect_edit_->toPlainText();
        p.device.after_connect_command = after_connect_edit_->toPlainText();
        p.device.before_job_command = before_job_edit_->toPlainText();
        p.device.after_job_command = after_job_edit_->toPlainText();
        p.repeat.steps = repeat_steps_spin_->value();
        p.repeat.closed_loop_distance = repeat_gap_spin_->value();
        p.min_line.min_jump = min_jump_spin_->value();
        p.min_line.min_path = min_path_spin_->value();
        p.min_line.min_edge = min_edge_spin_->value();
        p.min_line.min_shift = min_shift_spin_->value();
        if (QListWidgetItem* it = device_list_->item(index))
            it->setText(p.name);
        syncDeviceListRows();
        active_lbl_->setText(trInk("Aktywne urządzenie: %1").arg(p.name));
    }

    void updateCustomFieldsEnabled()
    {
        const bool en = custom_chk_->isChecked();
        mfg_edit_->setEnabled(en);
        model_edit_->setEnabled(en);
        driver_combo_->setEnabled(!en);
        updateProtocolLockedByPreset();
    }

    /// Gdy „Własne” jest wyłączone, protokół jest zsynchronizowany z presetem.
    void updateProtocolLockedByPreset()
    {
        if (!protocol_combo_ || !custom_chk_ || !driver_combo_)
            return;
        const bool locked = !custom_chk_->isChecked();
        if (locked) {
            DevicePreset preset;
            const QString pid = driver_combo_->currentData().toString();
            if (devicePresetById(pid, preset)) {
                const int idx = protocol_combo_->findData(int(preset.default_protocol));
                if (idx >= 0) {
                    QSignalBlocker b(protocol_combo_);
                    protocol_combo_->setCurrentIndex(idx);
                }
            }
        }
        protocol_combo_->setEnabled(!locked);
        protocol_combo_->setToolTip(
            locked ? trInk("Protokół jest ustawiany przez wybrany sterownik. "
                           "Zaznacz „Własne”, aby zmienić ręcznie.")
                   : QString());
    }

    void updateProtocolTabVisibility()
    {
        const bool is_gcode = true;
        gcode_group_->setVisible(true);
        const int lift = gcode_lift_combo_->currentData().toInt();
        const bool custom_lift = is_gcode && lift == int(GCodeProtocolSettings::Custom);
        const bool z_axis = is_gcode && lift == int(GCodeProtocolSettings::ZAxis);
        const bool solenoid = is_gcode && lift == int(GCodeProtocolSettings::SolenoidPwm);
        gcode_lift_edit_->setVisible(custom_lift);
        gcode_lower_edit_->setVisible(custom_lift);
        if (gcode_lift_edit_label_)
            gcode_lift_edit_label_->setVisible(custom_lift);
        if (gcode_lower_edit_label_)
            gcode_lower_edit_label_->setVisible(custom_lift);
        gcode_upper_z_spin_->setVisible(z_axis);
        gcode_lower_z_spin_->setVisible(z_axis);
        if (gcode_z_up_label_)
            gcode_z_up_label_->setVisible(z_axis);
        if (gcode_z_down_label_)
            gcode_z_down_label_->setVisible(z_axis);
        pwm_up_spin_->setVisible(solenoid);
        pwm_down_spin_->setVisible(solenoid);
        pwm_max_spin_->setVisible(solenoid);
        if (pwm_up_label_)
            pwm_up_label_->setVisible(solenoid);
        if (pwm_down_label_)
            pwm_down_label_->setVisible(solenoid);
        if (pwm_max_label_)
            pwm_max_label_->setVisible(solenoid);
    }

    void updateTransportVisibility()
    {
        const auto kind =
            static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
        const bool serial = kind == PlotTransportKind::SerialPort;
        const bool tcp = kind == PlotTransportKind::TcpIp;
        setFormRowVisible(conn_form_, port_row_widget_, serial);
        setFormRowVisible(conn_form_, serial_params_row_widget_, serial);
        setFormRowVisible(conn_form_, tcp_row_widget_, tcp);
        setFormRowVisible(conn_form_, flow_widget_, serial);
    }

    void refreshSerialPorts()
    {
        const QString wanted = port_combo_->currentData().toString();
        port_combo_->blockSignals(true);
        port_combo_->clear();
        for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
            QString label = info.portName();
            const QString desc = info.description();
            if (!desc.isEmpty())
                label += QStringLiteral(" — ") + desc;
            const QString path =
                info.systemLocation().isEmpty() ? info.portName() : info.systemLocation();
            port_combo_->addItem(label, path);
        }
        port_combo_->blockSignals(false);
        setSerialPortInCombo(wanted);
    }

    void setSerialPortInCombo(const QString& port_name)
    {
        if (port_name.isEmpty())
            return;
        int idx = port_combo_->findData(port_name);
        if (idx < 0)
            idx = port_combo_->findText(port_name, Qt::MatchExactly);
        if (idx >= 0) {
            port_combo_->setCurrentIndex(idx);
            return;
        }
        port_combo_->insertItem(0, port_name, port_name);
        port_combo_->setCurrentIndex(0);
    }

    void addDevice()
    {
        saveUiToProfile(current_);
        DeviceProfile p;
        DevicePreset preset;
        if (devicePresetById(QStringLiteral("generic"), preset))
            applyPresetToProfile(preset, p);
        p.name = trInk("Nowe urządzenie");
        profiles_.push_back(p);
        appendDeviceListItem(p.name);
        current_ = profiles_.size() - 1;
        loadProfileIntoUi(current_);
    }

    void removeDevice(int row)
    {
        if (profiles_.size() <= 1)
            return;
        profiles_.removeAt(row);
        delete device_list_->takeItem(row);
        current_ = std::min(current_, int(profiles_.size()) - 1);
        loadProfileIntoUi(current_);
    }

    void copyDevice(int row)
    {
        saveUiToProfile(current_);
        DeviceProfile p = profiles_[row];
        p.name += trInk(" (kopia)");
        profiles_.push_back(p);
        appendDeviceListItem(p.name);
        current_ = profiles_.size() - 1;
        loadProfileIntoUi(current_);
    }

    void showListMenu(const QPoint& pos)
    {
        const int row = device_list_->indexAt(pos).row();
        if (row < 0)
            return;
        QMenu menu(dlg_);
        menu.addAction(trInk("Usuń"), dlg_, [this, row]() { removeDevice(row); });
        menu.addAction(trInk("Kopiuj"), dlg_, [this, row]() { copyDevice(row); });
        menu.exec(device_list_->viewport()->mapToGlobal(pos));
    }

    QDialog* dlg_ = nullptr;
    PlotJobSettings& job_;
    DevicePluginLoader* plugins_ = nullptr;
    QVector<DeviceProfile> profiles_;
    int current_ = 0;
    bool syncing_ = false;

    QListWidget* device_list_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QLabel* active_lbl_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;

    QLineEdit* name_edit_ = nullptr;
    QComboBox* driver_combo_ = nullptr;
    QLineEdit* mfg_edit_ = nullptr;
    QLineEdit* model_edit_ = nullptr;
    QCheckBox* custom_chk_ = nullptr;

    QCheckBox* swap_chk_ = nullptr;
    QCheckBox* mirror_x_chk_ = nullptr;
    QCheckBox* mirror_y_chk_ = nullptr;
    QDoubleSpinBox* scale_spin_ = nullptr;
    QDoubleSpinBox* work_area_w_spin_ = nullptr;
    QDoubleSpinBox* work_area_h_spin_ = nullptr;

    QFormLayout* conn_form_ = nullptr;
    QComboBox* transport_combo_ = nullptr;
    QWidget* port_row_widget_ = nullptr;
    QComboBox* port_combo_ = nullptr;
    QPushButton* refresh_port_btn_ = nullptr;
    QWidget* serial_params_row_widget_ = nullptr;
    QSpinBox* baud_spin_ = nullptr;
    QComboBox* bytesize_combo_ = nullptr;
    QComboBox* parity_combo_ = nullptr;
    QComboBox* stopbits_combo_ = nullptr;
    QWidget* flow_widget_ = nullptr;
    QCheckBox* flow_rts_chk_ = nullptr;
    QCheckBox* flow_dsr_chk_ = nullptr;
    QCheckBox* flow_xon_chk_ = nullptr;
    QWidget* tcp_row_widget_ = nullptr;
    QLineEdit* tcp_host_edit_ = nullptr;
    QSpinBox* tcp_port_spin_ = nullptr;

    QComboBox* protocol_combo_ = nullptr;
    QGroupBox* gcode_group_ = nullptr;
    QCheckBox* gcode_builtin_chk_ = nullptr;
    QComboBox* gcode_dialect_combo_ = nullptr;
    QComboBox* gcode_lift_combo_ = nullptr;
    QSpinBox* gcode_precision_spin_ = nullptr;
    QDoubleSpinBox* gcode_upper_z_spin_ = nullptr;
    QDoubleSpinBox* gcode_lower_z_spin_ = nullptr;
    QLabel* gcode_z_up_label_ = nullptr;
    QLabel* gcode_z_down_label_ = nullptr;
    QSpinBox* pwm_up_spin_ = nullptr;
    QSpinBox* pwm_down_spin_ = nullptr;
    QSpinBox* pwm_max_spin_ = nullptr;
    QLabel* pwm_up_label_ = nullptr;
    QLabel* pwm_down_label_ = nullptr;
    QLabel* pwm_max_label_ = nullptr;
    QLabel* gcode_lift_edit_label_ = nullptr;
    QLabel* gcode_lower_edit_label_ = nullptr;

    QDoubleSpinBox* blade_offset_spin_ = nullptr;
    QDoubleSpinBox* blade_cutoff_spin_ = nullptr;
    QDoubleSpinBox* blade_quality_spin_ = nullptr;
    QDoubleSpinBox* overcut_spin_ = nullptr;
    QSpinBox* repeat_steps_spin_ = nullptr;
    QDoubleSpinBox* repeat_gap_spin_ = nullptr;
    QDoubleSpinBox* min_jump_spin_ = nullptr;
    QDoubleSpinBox* min_path_spin_ = nullptr;
    QDoubleSpinBox* min_edge_spin_ = nullptr;
    QDoubleSpinBox* min_shift_spin_ = nullptr;
    QDoubleSpinBox* closed_poly_eps_spin_ = nullptr;
    QPlainTextEdit* gcode_lift_edit_ = nullptr;
    QPlainTextEdit* gcode_lower_edit_ = nullptr;
    QPlainTextEdit* before_connect_edit_ = nullptr;
    QPlainTextEdit* after_connect_edit_ = nullptr;
    QPlainTextEdit* before_job_edit_ = nullptr;
    QPlainTextEdit* after_job_edit_ = nullptr;

    UiProfile ui_profile_ = UiProfile::Desktop;
};

} // namespace

void applyPersistedDeviceProfile(PlotJobSettings& job)
{
    const QVector<DeviceProfile> profiles = loadDeviceProfiles(job);
    if (profiles.isEmpty())
        return;
    const int idx = qBound(0, findProfileIndex(profiles, job), profiles.size() - 1);
    profileToJob(profiles[idx], job);
}

bool runDeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins)
{
    DeviceSetupDialog setup(parent, job, plugins);
    return setup.exec();
}

} // namespace inkcut
