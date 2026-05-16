// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_setup_dialog.hpp"

#include "app_settings.hpp"
#include "device_plugin.hpp"
#include "device_presets.hpp"
#include "filters.hpp"
#include "i18n.hpp"
#include "plugin_loader.hpp"
#include "ui_profile.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
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
    PlotProtocol protocol = PlotProtocol::HPGL;
    int dmpl_mode = 1;
    double plot_scale = 1021.0 / 90.0;
    bool hpgl_pad = false;
    int velocity = 120;
    bool gcode_builtin = true;
    GCodeProtocolSettings::Dialect gcode_dialect = GCodeProtocolSettings::Dialect::Generic;
    GCodeProtocolSettings::LiftMode gcode_lift = GCodeProtocolSettings::Implicit;
    int gcode_precision = 3;
    double gcode_upper_z = 5.0;
    double gcode_lower_z = 0.0;
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
    switch (p) {
    case PlotProtocol::HPGL:
        return QStringLiteral("HPGL");
    case PlotProtocol::DMPL:
        return QStringLiteral("DMPL");
    case PlotProtocol::GPGL:
        return QStringLiteral("GPGL");
    case PlotProtocol::GCode:
        return QStringLiteral("G-code");
    case PlotProtocol::CAMM_GL1:
        return QStringLiteral("CAMM GL-1");
    }
    return QStringLiteral("HPGL");
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
    combo->addItem(QStringLiteral("(brak)"), QString());
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
    p.protocol = job.protocol.protocol;
    p.dmpl_mode = job.protocol.dmpl_mode;
    p.plot_scale = job.protocol.plot_scale;
    p.hpgl_pad = job.protocol.hpgl_pad;
    p.velocity = job.velocity;
    p.gcode_builtin = job.protocol.gcode.use_builtin;
    p.gcode_dialect = job.protocol.gcode.dialect;
    p.gcode_lift = job.protocol.gcode.lift_mode;
    p.gcode_precision = job.protocol.gcode.precision;
    p.gcode_upper_z = job.protocol.gcode.upper_z;
    p.gcode_lower_z = job.protocol.gcode.lower_z;
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
    job.material.width = p.width;
    job.material.height = p.height;
    job.protocol.protocol = p.protocol;
    job.protocol.dmpl_mode = p.dmpl_mode;
    job.protocol.plot_scale = p.plot_scale;
    job.protocol.hpgl_pad = p.hpgl_pad;
    job.velocity = p.velocity;
    job.protocol.gcode.use_builtin = p.gcode_builtin;
    job.protocol.gcode.dialect = p.gcode_dialect;
    job.protocol.gcode.lift_mode = p.gcode_lift;
    job.protocol.gcode.precision = p.gcode_precision;
    job.protocol.gcode.upper_z = p.gcode_upper_z;
    job.protocol.gcode.lower_z = p.gcode_lower_z;
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
    p.dmpl_mode = preset.dmpl_mode;
    if (preset.default_protocol == PlotProtocol::GCode)
        p.plot_scale = 1.0;
    p.width = preset.material_width;
    p.height = preset.material_height;
    if (p.name.isEmpty() || p.name == trInk("Nowe urządzenie"))
        p.name = QStringLiteral("%1 %2").arg(preset.manufacturer, preset.model);
}

QJsonObject profileToJson(const DeviceProfile& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), p.name);
    o.insert(QStringLiteral("custom"), p.custom);
    o.insert(QStringLiteral("preset_id"), p.device.preset_id);
    o.insert(QStringLiteral("transport"), int(p.device.transport));
    o.insert(QStringLiteral("port"), p.device.port_name);
    o.insert(QStringLiteral("baud"), int(p.device.baud_rate));
    o.insert(QStringLiteral("output"), p.device.output_path);
    o.insert(QStringLiteral("printer"), p.device.printer_name);
    o.insert(QStringLiteral("swap_xy"), p.device.swap_xy);
    o.insert(QStringLiteral("mirror_x"), p.device.mirror_x);
    o.insert(QStringLiteral("mirror_y"), p.device.mirror_y);
    o.insert(QStringLiteral("device_scale"), p.device.device_scale);
    o.insert(QStringLiteral("width"), p.width);
    o.insert(QStringLiteral("height"), p.height);
    o.insert(QStringLiteral("protocol"), int(p.protocol));
    o.insert(QStringLiteral("dmpl_mode"), p.dmpl_mode);
    o.insert(QStringLiteral("plot_scale"), p.plot_scale);
    o.insert(QStringLiteral("hpgl_pad"), p.hpgl_pad);
    o.insert(QStringLiteral("velocity"), p.velocity);
    o.insert(QStringLiteral("gcode_builtin"), p.gcode_builtin);
    o.insert(QStringLiteral("gcode_dialect"), int(p.gcode_dialect));
    o.insert(QStringLiteral("gcode_lift"), int(p.gcode_lift));
    o.insert(QStringLiteral("gcode_precision"), p.gcode_precision);
    o.insert(QStringLiteral("gcode_upper_z"), p.gcode_upper_z);
    o.insert(QStringLiteral("gcode_lower_z"), p.gcode_lower_z);
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
    p.device.transport =
        static_cast<PlotTransportKind>(o.value(QStringLiteral("transport")).toInt());
    p.device.port_name = o.value(QStringLiteral("port")).toString();
    p.device.baud_rate = o.value(QStringLiteral("baud")).toInt(9600);
    p.device.output_path = o.value(QStringLiteral("output")).toString();
    p.device.printer_name = o.value(QStringLiteral("printer")).toString();
    p.device.swap_xy = o.value(QStringLiteral("swap_xy")).toBool();
    p.device.mirror_x = o.value(QStringLiteral("mirror_x")).toBool();
    p.device.mirror_y = o.value(QStringLiteral("mirror_y")).toBool();
    p.device.device_scale = o.value(QStringLiteral("device_scale")).toDouble(1.0);
    p.width = o.value(QStringLiteral("width")).toDouble(600);
    p.height = o.value(QStringLiteral("height")).toDouble(400);
    p.protocol = static_cast<PlotProtocol>(o.value(QStringLiteral("protocol")).toInt());
    p.dmpl_mode = o.value(QStringLiteral("dmpl_mode")).toInt(1);
    p.plot_scale = o.value(QStringLiteral("plot_scale")).toDouble(1021.0 / 90.0);
    p.hpgl_pad = o.value(QStringLiteral("hpgl_pad")).toBool();
    p.velocity = o.value(QStringLiteral("velocity")).toInt(120);
    p.gcode_builtin = o.value(QStringLiteral("gcode_builtin")).toBool(true);
    p.gcode_dialect = static_cast<GCodeProtocolSettings::Dialect>(
        o.value(QStringLiteral("gcode_dialect")).toInt());
    p.gcode_lift = static_cast<GCodeProtocolSettings::LiftMode>(
        o.value(QStringLiteral("gcode_lift")).toInt());
    p.gcode_precision = o.value(QStringLiteral("gcode_precision")).toInt(3);
    p.gcode_upper_z = o.value(QStringLiteral("gcode_upper_z")).toDouble(5.0);
    p.gcode_lower_z = o.value(QStringLiteral("gcode_lower_z")).toDouble();
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
    DevicePreset preset;
    if (devicePresetById(p.device.preset_id, preset)) {
        p.device.manufacturer = preset.manufacturer;
        p.device.model_name = preset.model;
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

void saveDeviceProfiles(const QVector<DeviceProfile>& profiles)
{
    QJsonArray arr;
    for (const DeviceProfile& p : profiles)
        arr.append(profileToJson(p));
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    settings.setValue(QStringLiteral("device_profiles_v1"),
                      QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

class DeviceSetupDialog final {
public:
    DeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins)
        : job_(job)
        , plugins_(plugins)
    {
        profiles_ = loadDeviceProfiles(job);
        current_ = 0;
        for (int i = 0; i < profiles_.size(); ++i) {
            if (profiles_[i].device.preset_id == job.device.preset_id) {
                current_ = i;
                break;
            }
        }
        profiles_[current_] = profileFromJob(job);

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

        add_btn_ = new QPushButton(QStringLiteral("+ Dodaj"), dlg_);
        auto* left = new QVBoxLayout();
        left->addWidget(new QLabel(trInk("Dostępne urządzenia"), dlg_));
        left->addWidget(device_list_, 1);
        left->addWidget(add_btn_);

        tabs_ = new QTabWidget(dlg_);

        // --- Ogólne ---
        auto* general = new QWidget(tabs_);
        auto* gform = new QFormLayout(general);
        styleFormLayout(gform, ui_profile_);
        name_edit_ = new QLineEdit(general);
        driver_combo_ = new QComboBox(general);
        fillPresetCombo(driver_combo_);
        mfg_edit_ = new QLineEdit(general);
        model_edit_ = new QLineEdit(general);
        width_spin_ = new QDoubleSpinBox(general);
        height_spin_ = new QDoubleSpinBox(general);
        for (QDoubleSpinBox* s : {width_spin_, height_spin_}) {
            s->setRange(0.1, 99999.9);
            s->setDecimals(2);
            s->setSuffix(QStringLiteral(" mm"));
        }
        custom_chk_ = new QCheckBox(trInk("Własne"), general);
        gform->addRow(QStringLiteral("Nazwa"), name_edit_);
        gform->addRow(QStringLiteral("Sterownik"), driver_combo_);
        gform->addRow(QStringLiteral("Producent"), mfg_edit_);
        gform->addRow(QStringLiteral("Model"), model_edit_);
        gform->addRow(trInk("Szerokość"), width_spin_);
        gform->addRow(trInk("Długość"), height_spin_);
        gform->addRow(QString(), custom_chk_);
        tabs_->addTab(wrapTabInScroll(general), trInk("Ogólne"));

        // --- Urządzenie ---
        auto* device_tab = new QWidget(tabs_);
        auto* dform = new QFormLayout(device_tab);
        styleFormLayout(dform, ui_profile_);
        swap_chk_ = new QCheckBox(trInk("Zamień X/Y"), device_tab);
        mirror_x_chk_ = new QCheckBox(QStringLiteral("Lustro X"), device_tab);
        mirror_y_chk_ = new QCheckBox(QStringLiteral("Lustro Y"), device_tab);
        scale_spin_ = new QDoubleSpinBox(device_tab);
        scale_spin_->setRange(0.001, 1000);
        scale_spin_->setDecimals(6);
        dform->addRow(swap_chk_);
        dform->addRow(mirror_x_chk_);
        dform->addRow(mirror_y_chk_);
        dform->addRow(trInk("Skala wyjścia"), scale_spin_);
        tabs_->addTab(wrapTabInScroll(device_tab), trInk("Urządzenie"));

        // --- Połączenie ---
        auto* conn = new QWidget(tabs_);
        auto* cform = new QFormLayout(conn);
        styleFormLayout(cform, ui_profile_);
        transport_combo_ = new QComboBox(conn);
        transport_combo_->addItem(QStringLiteral("Port szeregowy"), int(PlotTransportKind::SerialPort));
        transport_combo_->addItem(QStringLiteral("Zapis do pliku"), int(PlotTransportKind::FileOutput));
        transport_combo_->addItem(QStringLiteral("Drukarka (CUPS)"), int(PlotTransportKind::Printer));
        port_edit_ = new QLineEdit(conn);
        baud_spin_ = new QSpinBox(conn);
        baud_spin_->setRange(1200, 1000000);
        output_edit_ = new QLineEdit(conn);
        printer_edit_ = new QLineEdit(conn);
        printer_edit_->setPlaceholderText(QStringLiteral("np. HP_Deskjet (lp -d)"));
        browse_btn_ = new QPushButton(QStringLiteral("…"), conn);
        auto* out_row = new QHBoxLayout();
        out_row->addWidget(output_edit_, 1);
        out_row->addWidget(browse_btn_);
        plugin_combo_ = new QComboBox(conn);
        fillPluginCombo(plugin_combo_, plugins_);
        probe_btn_ = new QPushButton(QStringLiteral("Sonduj porty"), conn);
        cform->addRow(QStringLiteral("Typ"), transport_combo_);
        cform->addRow(trInk("Port / urządzenie"), port_edit_);
        cform->addRow(QStringLiteral("Baud"), baud_spin_);
        cform->addRow(trInk("Plik wyjściowy"), out_row);
        cform->addRow(QStringLiteral("Drukarka CUPS"), printer_edit_);
        cform->addRow(QStringLiteral("Wtyczka"), plugin_combo_);
        cform->addRow(QString(), probe_btn_);
        after_connect_edit_ = new QPlainTextEdit(conn);
        after_connect_edit_->setMaximumHeight(48);
        after_connect_edit_->setPlaceholderText(trInk("Po połączeniu (np. G21\\n)"));
        before_job_edit_ = new QPlainTextEdit(conn);
        before_job_edit_->setMaximumHeight(48);
        before_job_edit_->setPlaceholderText(trInk("Przed cięciem"));
        after_job_edit_ = new QPlainTextEdit(conn);
        after_job_edit_->setMaximumHeight(48);
        after_job_edit_->setPlaceholderText(QStringLiteral("Po zadaniu"));
        cform->addRow(trInk("Po połączeniu"), after_connect_edit_);
        cform->addRow(QStringLiteral("Przed zadaniem"), before_job_edit_);
        cform->addRow(QStringLiteral("Po zadaniu"), after_job_edit_);
        tabs_->addTab(wrapTabInScroll(conn), trInk("Połączenie"));

        // --- Protokół ---
        auto* proto = new QWidget(tabs_);
        auto* pform = new QFormLayout(proto);
        styleFormLayout(pform, ui_profile_);
        protocol_combo_ = new QComboBox(proto);
        protocol_combo_->addItem(protocolLabel(PlotProtocol::HPGL), int(PlotProtocol::HPGL));
        protocol_combo_->addItem(protocolLabel(PlotProtocol::DMPL), int(PlotProtocol::DMPL));
        protocol_combo_->addItem(protocolLabel(PlotProtocol::GPGL), int(PlotProtocol::GPGL));
        protocol_combo_->addItem(protocolLabel(PlotProtocol::GCode), int(PlotProtocol::GCode));
        protocol_combo_->addItem(protocolLabel(PlotProtocol::CAMM_GL1), int(PlotProtocol::CAMM_GL1));
        dmpl_spin_ = new QSpinBox(proto);
        dmpl_spin_->setRange(1, 6);
        plot_scale_spin_ = new QDoubleSpinBox(proto);
        plot_scale_spin_->setRange(0.001, 100000);
        plot_scale_spin_->setDecimals(4);
        velocity_spin_ = new QSpinBox(proto);
        velocity_spin_->setRange(1, 99999);
        hpgl_pad_chk_ = new QCheckBox(trInk("Dopełnianie linii (HPGL pad)"), proto);
        gcode_builtin_chk_ = new QCheckBox(QStringLiteral("Wbudowane komendy start/stop"), proto);
        gcode_lift_combo_ = new QComboBox(proto);
        gcode_lift_combo_->addItem(QStringLiteral("Implicit (G00/G01)"),
                                   int(GCodeProtocolSettings::Implicit));
        gcode_lift_combo_->addItem(trInk("Oś Z"),
                                   int(GCodeProtocolSettings::ZAxis));
        gcode_lift_combo_->addItem(trInk("Własne G-code"),
                                   int(GCodeProtocolSettings::Custom));
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
        dmpl_row_label_ = new QLabel(QStringLiteral("Tryb DMPL"), proto);
        plot_scale_row_label_ = new QLabel(QStringLiteral("Skala plotera"), proto);
        velocity_row_label_ = new QLabel(trInk("Prędkość (VS/!V)"), proto);
        hpgl_pad_row_widget_ = hpgl_pad_chk_;
        gcode_group_ = new QGroupBox(QStringLiteral("G-code / GRBL"), proto);
        auto* gcode_form = new QFormLayout(gcode_group_);
        gcode_form->addRow(gcode_builtin_chk_);
        gcode_form->addRow(QStringLiteral("Dialekt"), gcode_dialect_combo_);
        gcode_form->addRow(trInk("Podnoszenie narzędzia"), gcode_lift_combo_);
        gcode_form->addRow(QStringLiteral("Precyzja"), gcode_precision_spin_);
        gcode_form->addRow(trInk("Z góra (mm)"), gcode_upper_z_spin_);
        gcode_form->addRow(trInk("Z dół (mm)"), gcode_lower_z_spin_);
        gcode_lift_edit_ = new QPlainTextEdit(gcode_group_);
        gcode_lift_edit_->setMaximumHeight(56);
        gcode_lift_edit_->setPlaceholderText(trInk("G-code podniesienia (tryb Własne)"));
        gcode_lower_edit_ = new QPlainTextEdit(gcode_group_);
        gcode_lower_edit_->setMaximumHeight(56);
        gcode_lower_edit_->setPlaceholderText(trInk("G-code opuszczenia (tryb Własne)"));
        gcode_form->addRow(QStringLiteral("Lift G-code"), gcode_lift_edit_);
        gcode_form->addRow(QStringLiteral("Lower G-code"), gcode_lower_edit_);

        pform->addRow(trInk("Język"), protocol_combo_);
        pform->addRow(dmpl_row_label_, dmpl_spin_);
        pform->addRow(plot_scale_row_label_, plot_scale_spin_);
        pform->addRow(velocity_row_label_, velocity_spin_);
        pform->addRow(hpgl_pad_row_widget_);
        pform->addRow(gcode_group_);
        tabs_->addTab(wrapTabInScroll(proto), trInk("Protokół"));

        // --- Filtry urządzenia ---
        auto* filters = new QWidget(tabs_);
        auto* filters_scroll = new QScrollArea(filters);
        filters_scroll->setWidgetResizable(true);
        auto* filters_inner = new QWidget(filters_scroll);
        auto* fv = new QVBoxLayout(filters_inner);

        auto* blade_gb = new QGroupBox(QStringLiteral("Offset ostrza"), filters_inner);
        auto* blade_form = new QFormLayout(blade_gb);
        blade_offset_spin_ = new QDoubleSpinBox(blade_gb);
        blade_offset_spin_->setRange(0, 50);
        blade_offset_spin_->setDecimals(3);
        blade_cutoff_spin_ = new QDoubleSpinBox(blade_gb);
        blade_cutoff_spin_->setRange(0, 90);
        blade_cutoff_spin_->setDecimals(1);
        blade_quality_spin_ = new QDoubleSpinBox(blade_gb);
        blade_quality_spin_->setRange(0.001, 100);
        blade_form->addRow(QStringLiteral("Offset"), blade_offset_spin_);
        blade_form->addRow(trInk("Kąt odcięcia"), blade_cutoff_spin_);
        blade_form->addRow(trInk("Jakość"), blade_quality_spin_);

        auto* overcut_gb = new QGroupBox(QStringLiteral("Overcut"), filters_inner);
        auto* overcut_form = new QFormLayout(overcut_gb);
        overcut_spin_ = new QDoubleSpinBox(overcut_gb);
        overcut_spin_->setRange(0, 50);
        overcut_spin_->setDecimals(3);
        overcut_form->addRow(QStringLiteral("Overcut"), overcut_spin_);
        closed_poly_eps_spin_ = new QDoubleSpinBox(overcut_gb);
        closed_poly_eps_spin_->setRange(0.001, 50);
        closed_poly_eps_spin_->setDecimals(3);
        closed_poly_eps_spin_->setValue(0.25);
        overcut_form->addRow(trInk("Próg zamknięcia"), closed_poly_eps_spin_);

        auto* min_gb = new QGroupBox(QStringLiteral("Min. linia"), filters_inner);
        auto* min_form = new QFormLayout(min_gb);
        min_jump_spin_ = new QDoubleSpinBox(min_gb);
        min_path_spin_ = new QDoubleSpinBox(min_gb);
        min_shift_spin_ = new QDoubleSpinBox(min_gb);
        min_edge_spin_ = new QDoubleSpinBox(min_gb);
        for (QDoubleSpinBox* s : {min_jump_spin_, min_path_spin_, min_shift_spin_, min_edge_spin_}) {
            s->setRange(0, 1000);
            s->setDecimals(3);
        }
        min_form->addRow(QStringLiteral("Min. skok (PU)"), min_jump_spin_);
        min_form->addRow(trInk("Min. ścieżka"), min_path_spin_);
        min_form->addRow(trInk("Min. przesunięcie"), min_shift_spin_);
        min_form->addRow(trInk("Min. krawędź"), min_edge_spin_);

        auto* repeat_gb = new QGroupBox(trInk("Powtórzenia (cały job)"), filters_inner);
        auto* repeat_form = new QFormLayout(repeat_gb);
        styleFormLayout(repeat_form, ui_profile_);
        repeat_steps_spin_ = new QSpinBox(repeat_gb);
        repeat_steps_spin_->setRange(1, 50);
        repeat_gap_spin_ = new QDoubleSpinBox(repeat_gb);
        repeat_gap_spin_->setRange(0, 100);
        repeat_gap_spin_->setDecimals(3);
        repeat_form->addRow(trInk("Powtórzenia"), repeat_steps_spin_);
        repeat_form->addRow(trInk("Max. luka zamknięcia"), repeat_gap_spin_);
        auto* repeat_hint = new QLabel(
            QStringLiteral("Powtórzenia pojedynczej warstwy (np. twardy materiał) ustaw w głównym "
                           "oknie: lewy panel → zakładka Warstwy."),
            repeat_gb);
        repeat_hint->setWordWrap(true);
        repeat_form->addRow(repeat_hint);

        fv->addWidget(blade_gb);
        fv->addWidget(overcut_gb);
        fv->addWidget(min_gb);
        fv->addWidget(repeat_gb);
        fv->addStretch(1);
        filters_scroll->setWidget(filters_inner);
        auto* filters_root = new QVBoxLayout(filters);
        filters_root->addWidget(filters_scroll);
        tabs_->addTab(filters, QStringLiteral("Filtry"));

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
                         });
        QObject::connect(transport_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateTransportVisibility(); });
        QObject::connect(browse_btn_, &QPushButton::clicked, dlg_, [this]() {
            const QString path = QFileDialog::getSaveFileName(
                dlg_, trInk("Plik wyjściowy"), output_edit_->text(),
                QStringLiteral("Program (*.hpgl *.plt *.prn);;Wszystkie (*)"));
            if (!path.isEmpty())
                output_edit_->setText(path);
        });
        QObject::connect(probe_btn_, &QPushButton::clicked, dlg_, [this]() { probePorts(); });
        QObject::connect(protocol_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateProtocolTabVisibility(); });
        QObject::connect(gcode_lift_combo_, qOverload<int>(&QComboBox::currentIndexChanged), dlg_,
                         [this]() { updateProtocolTabVisibility(); });
        QObject::connect(buttons_, &QDialogButtonBox::accepted, dlg_, [this]() {
            saveUiToProfile(current_);
            profiles_[current_].plugin_id = plugin_combo_->currentData().toString();
            saveDeviceProfiles(profiles_);
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
        width_spin_->setValue(p.width);
        height_spin_->setValue(p.height);
        custom_chk_->setChecked(p.custom);
        swap_chk_->setChecked(p.device.swap_xy);
        mirror_x_chk_->setChecked(p.device.mirror_x);
        mirror_y_chk_->setChecked(p.device.mirror_y);
        scale_spin_->setValue(p.device.device_scale);
        transport_combo_->setCurrentIndex(
            transport_combo_->findData(int(p.device.transport)));
        port_edit_->setText(p.device.port_name);
        baud_spin_->setValue(p.device.baud_rate);
        output_edit_->setText(p.device.output_path);
        printer_edit_->setText(p.device.printer_name);
        protocol_combo_->setCurrentIndex(protocol_combo_->findData(int(p.protocol)));
        dmpl_spin_->setValue(p.dmpl_mode);
        plot_scale_spin_->setValue(p.plot_scale);
        velocity_spin_->setValue(p.velocity);
        hpgl_pad_chk_->setChecked(p.hpgl_pad);
        gcode_builtin_chk_->setChecked(p.gcode_builtin);
        gcode_dialect_combo_->setCurrentIndex(
            gcode_dialect_combo_->findData(int(p.gcode_dialect)));
        gcode_lift_combo_->setCurrentIndex(gcode_lift_combo_->findData(int(p.gcode_lift)));
        gcode_precision_spin_->setValue(p.gcode_precision);
        gcode_upper_z_spin_->setValue(p.gcode_upper_z);
        gcode_lower_z_spin_->setValue(p.gcode_lower_z);
        blade_offset_spin_->setValue(p.blade.offset);
        blade_cutoff_spin_->setValue(p.blade.cutoff_deg);
        blade_quality_spin_->setValue(p.blade.quality_factor);
        overcut_spin_->setValue(p.overcut);
        closed_poly_eps_spin_->setValue(p.closed_poly_eps);
        gcode_lift_edit_->setPlainText(p.gcode_lift_gcode);
        gcode_lower_edit_->setPlainText(p.gcode_lower_gcode);
        after_connect_edit_->setPlainText(p.device.after_connect_command);
        before_job_edit_->setPlainText(p.device.before_job_command);
        after_job_edit_->setPlainText(p.device.after_job_command);
        const int pi = plugin_combo_->findData(p.plugin_id);
        if (pi >= 0)
            plugin_combo_->setCurrentIndex(pi);
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
        p.width = width_spin_->value();
        p.height = height_spin_->value();
        p.device.swap_xy = swap_chk_->isChecked();
        p.device.mirror_x = mirror_x_chk_->isChecked();
        p.device.mirror_y = mirror_y_chk_->isChecked();
        p.device.device_scale = scale_spin_->value();
        p.device.transport =
            static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
        p.device.port_name = port_edit_->text().trimmed();
        p.device.baud_rate = baud_spin_->value();
        p.device.output_path = output_edit_->text().trimmed();
        p.device.printer_name = printer_edit_->text().trimmed();
        p.protocol = static_cast<PlotProtocol>(protocol_combo_->currentData().toInt());
        p.dmpl_mode = dmpl_spin_->value();
        p.plot_scale = plot_scale_spin_->value();
        p.velocity = velocity_spin_->value();
        p.hpgl_pad = hpgl_pad_chk_->isChecked();
        p.gcode_builtin = gcode_builtin_chk_->isChecked();
        p.gcode_dialect = static_cast<GCodeProtocolSettings::Dialect>(
            gcode_dialect_combo_->currentData().toInt());
        p.gcode_lift = static_cast<GCodeProtocolSettings::LiftMode>(
            gcode_lift_combo_->currentData().toInt());
        p.gcode_precision = gcode_precision_spin_->value();
        p.gcode_upper_z = gcode_upper_z_spin_->value();
        p.gcode_lower_z = gcode_lower_z_spin_->value();
        p.blade.offset = blade_offset_spin_->value();
        p.blade.cutoff_deg = blade_cutoff_spin_->value();
        p.blade.quality_factor = blade_quality_spin_->value();
        p.overcut = overcut_spin_->value();
        p.closed_poly_eps = closed_poly_eps_spin_->value();
        p.gcode_lift_gcode = gcode_lift_edit_->toPlainText();
        p.gcode_lower_gcode = gcode_lower_edit_->toPlainText();
        p.device.after_connect_command = after_connect_edit_->toPlainText();
        p.device.before_job_command = before_job_edit_->toPlainText();
        p.device.after_job_command = after_job_edit_->toPlainText();
        p.plugin_id = plugin_combo_->currentData().toString();
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
        width_spin_->setEnabled(en);
        height_spin_->setEnabled(en);
        driver_combo_->setEnabled(!en);
    }

    void updateProtocolTabVisibility()
    {
        const auto proto =
            static_cast<PlotProtocol>(protocol_combo_->currentData().toInt());
        const bool is_hpgl = proto == PlotProtocol::HPGL;
        const bool is_dmpl = proto == PlotProtocol::DMPL;
        const bool is_gcode = proto == PlotProtocol::GCode;
        dmpl_row_label_->setVisible(is_dmpl);
        dmpl_spin_->setVisible(is_dmpl);
        hpgl_pad_chk_->setVisible(is_hpgl);
        gcode_group_->setVisible(is_gcode);
        const bool custom_lift =
            is_gcode && gcode_lift_combo_->currentData().toInt() ==
                           int(GCodeProtocolSettings::Custom);
        gcode_lift_edit_->setVisible(custom_lift);
        gcode_lower_edit_->setVisible(custom_lift);
    }

    void updateTransportVisibility()
    {
        const auto kind =
            static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
        const bool serial = kind == PlotTransportKind::SerialPort;
        const bool file_like =
            kind == PlotTransportKind::FileOutput || kind == PlotTransportKind::Printer;
        port_edit_->setEnabled(serial);
        baud_spin_->setEnabled(serial);
        output_edit_->setVisible(file_like);
        browse_btn_->setVisible(file_like);
        printer_edit_->setVisible(kind == PlotTransportKind::Printer);
    }

    void probePorts()
    {
        if (!plugins_)
            return;
        const QString pid = plugin_combo_->currentData().toString();
        for (DevicePlugin* p : plugins_->plugins()) {
            if (!pid.isEmpty() && p->pluginId() != pid)
                continue;
            const auto ports = p->probeSerialPorts();
            if (!ports.isEmpty()) {
                port_edit_->setText(ports.front().port_name);
                return;
            }
        }
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
        p.name += QStringLiteral(" (kopia)");
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
        menu.addAction(QStringLiteral("Kopiuj"), dlg_, [this, row]() { copyDevice(row); });
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
    QDoubleSpinBox* width_spin_ = nullptr;
    QDoubleSpinBox* height_spin_ = nullptr;
    QCheckBox* custom_chk_ = nullptr;

    QCheckBox* swap_chk_ = nullptr;
    QCheckBox* mirror_x_chk_ = nullptr;
    QCheckBox* mirror_y_chk_ = nullptr;
    QDoubleSpinBox* scale_spin_ = nullptr;

    QComboBox* transport_combo_ = nullptr;
    QLineEdit* port_edit_ = nullptr;
    QSpinBox* baud_spin_ = nullptr;
    QLineEdit* output_edit_ = nullptr;
    QLineEdit* printer_edit_ = nullptr;
    QPushButton* browse_btn_ = nullptr;
    QComboBox* plugin_combo_ = nullptr;
    QPushButton* probe_btn_ = nullptr;

    QComboBox* protocol_combo_ = nullptr;
    QSpinBox* dmpl_spin_ = nullptr;
    QDoubleSpinBox* plot_scale_spin_ = nullptr;
    QSpinBox* velocity_spin_ = nullptr;
    QCheckBox* hpgl_pad_chk_ = nullptr;
    QLabel* dmpl_row_label_ = nullptr;
    QLabel* plot_scale_row_label_ = nullptr;
    QLabel* velocity_row_label_ = nullptr;
    QWidget* hpgl_pad_row_widget_ = nullptr;
    QGroupBox* gcode_group_ = nullptr;
    QCheckBox* gcode_builtin_chk_ = nullptr;
    QComboBox* gcode_dialect_combo_ = nullptr;
    QComboBox* gcode_lift_combo_ = nullptr;
    QSpinBox* gcode_precision_spin_ = nullptr;
    QDoubleSpinBox* gcode_upper_z_spin_ = nullptr;
    QDoubleSpinBox* gcode_lower_z_spin_ = nullptr;

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
    QPlainTextEdit* after_connect_edit_ = nullptr;
    QPlainTextEdit* before_job_edit_ = nullptr;
    QPlainTextEdit* after_job_edit_ = nullptr;

    UiProfile ui_profile_ = UiProfile::Desktop;
};

} // namespace

bool runDeviceSetupDialog(QWidget* parent, PlotJobSettings& job, DevicePluginLoader* plugins)
{
    DeviceSetupDialog setup(parent, job, plugins);
    return setup.exec();
}

} // namespace inkcut
