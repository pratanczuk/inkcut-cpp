// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_export.hpp"

#include "job_pipeline.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace inkcut {

namespace {

QJsonObject protocolSettingsToJson(const ProtocolSettings& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("protocol"), plotProtocolToCli(p.protocol));
    o.insert(QStringLiteral("hpgl_pad"), p.hpgl_pad);
    o.insert(QStringLiteral("dmpl_mode"), p.dmpl_mode);
    o.insert(QStringLiteral("plot_scale"), p.plot_scale);

    QJsonObject gc;
    gc.insert(QStringLiteral("use_builtin"), p.gcode.use_builtin);
    gc.insert(QStringLiteral("dialect"), int(p.gcode.dialect));
    gc.insert(QStringLiteral("lift_mode"), int(p.gcode.lift_mode));
    gc.insert(QStringLiteral("precision"), p.gcode.precision);
    gc.insert(QStringLiteral("lower_z"), p.gcode.lower_z);
    gc.insert(QStringLiteral("upper_z"), p.gcode.upper_z);
    gc.insert(QStringLiteral("lift_gcode"), p.gcode.lift_gcode);
    gc.insert(QStringLiteral("lower_gcode"), p.gcode.lower_gcode);
    o.insert(QStringLiteral("gcode"), gc);
    return o;
}

bool protocolSettingsFromJson(const QJsonObject& o, ProtocolSettings& p, QString* err)
{
    const QString prot = o.value(QStringLiteral("protocol")).toString();
    if (!prot.isEmpty())
        p.protocol = plotProtocolFromCli(QStringView(prot));

    if (o.contains(QStringLiteral("hpgl_pad")))
        p.hpgl_pad = o.value(QStringLiteral("hpgl_pad")).toBool();
    if (o.contains(QStringLiteral("dmpl_mode")))
        p.dmpl_mode = o.value(QStringLiteral("dmpl_mode")).toInt();
    if (o.contains(QStringLiteral("plot_scale")))
        p.plot_scale = o.value(QStringLiteral("plot_scale")).toDouble();

    const QJsonObject gc = o.value(QStringLiteral("gcode")).toObject();
    if (!gc.isEmpty()) {
        if (gc.contains(QStringLiteral("use_builtin")))
            p.gcode.use_builtin = gc.value(QStringLiteral("use_builtin")).toBool();
        if (gc.contains(QStringLiteral("dialect")))
            p.gcode.dialect = static_cast<GCodeProtocolSettings::Dialect>(
                gc.value(QStringLiteral("dialect")).toInt());
        if (gc.contains(QStringLiteral("lift_mode")))
            p.gcode.lift_mode =
                static_cast<GCodeProtocolSettings::LiftMode>(gc.value(QStringLiteral("lift_mode")).toInt());
        if (gc.contains(QStringLiteral("precision")))
            p.gcode.precision = gc.value(QStringLiteral("precision")).toInt();
        if (gc.contains(QStringLiteral("lower_z")))
            p.gcode.lower_z = gc.value(QStringLiteral("lower_z")).toDouble();
        if (gc.contains(QStringLiteral("upper_z")))
            p.gcode.upper_z = gc.value(QStringLiteral("upper_z")).toDouble();
        if (gc.contains(QStringLiteral("lift_gcode")))
            p.gcode.lift_gcode = gc.value(QStringLiteral("lift_gcode")).toString();
        if (gc.contains(QStringLiteral("lower_gcode")))
            p.gcode.lower_gcode = gc.value(QStringLiteral("lower_gcode")).toString();
    }

    Q_UNUSED(err);
    return true;
}

QJsonObject plotJobSettingsToJson(const PlotJobSettings& s)
{
    QJsonObject o;
    o.insert(QStringLiteral("order"), orderStrategyToCli(s.order));
    o.insert(QStringLiteral("protocol_block"), protocolSettingsToJson(s.protocol));
    o.insert(QStringLiteral("flatten_step"), s.flatten_step);
    o.insert(QStringLiteral("velocity"), s.velocity);
    o.insert(QStringLiteral("repeat_steps"), s.repeat.steps);
    o.insert(QStringLiteral("repeat_gap"), s.repeat.closed_loop_distance);
    o.insert(QStringLiteral("min_jump"), s.min_line.min_jump);
    o.insert(QStringLiteral("min_path"), s.min_line.min_path);
    o.insert(QStringLiteral("min_edge"), s.min_line.min_edge);
    o.insert(QStringLiteral("min_shift"), s.min_line.min_shift);
    o.insert(QStringLiteral("blade_offset"), s.blade.offset);
    o.insert(QStringLiteral("blade_cutoff_deg"), s.blade.cutoff_deg);
    o.insert(QStringLiteral("blade_quality"), s.blade.quality_factor);
    o.insert(QStringLiteral("overcut"), s.overcut);
    o.insert(QStringLiteral("closed_poly_eps"), s.closed_poly_eps);

    QJsonObject mat;
    mat.insert(QStringLiteral("width"), s.material.width);
    mat.insert(QStringLiteral("height"), s.material.height);
    mat.insert(QStringLiteral("pad_l"), s.material.padding_left);
    mat.insert(QStringLiteral("pad_t"), s.material.padding_top);
    mat.insert(QStringLiteral("pad_r"), s.material.padding_right);
    mat.insert(QStringLiteral("pad_b"), s.material.padding_bottom);
    mat.insert(QStringLiteral("is_roll"), s.material.is_roll);
    mat.insert(QStringLiteral("cost_per_area"), s.material.cost_per_area);
    mat.insert(QStringLiteral("use_custom_force_speed"), s.material.use_custom_force_speed);
    mat.insert(QStringLiteral("force"), s.material.force);
    mat.insert(QStringLiteral("speed"), s.material.speed);
    o.insert(QStringLiteral("material"), mat);
    o.insert(QStringLiteral("plugin_id"), s.plugin_id);

    QJsonObject lay;
    lay.insert(QStringLiteral("scale_x"), s.layout.scale_x);
    lay.insert(QStringLiteral("scale_y"), s.layout.scale_y);
    lay.insert(QStringLiteral("mirror_x"), s.layout.mirror_x);
    lay.insert(QStringLiteral("mirror_y"), s.layout.mirror_y);
    lay.insert(QStringLiteral("rotation"), s.layout.rotation_deg);
    lay.insert(QStringLiteral("copies"), s.layout.copies);
    lay.insert(QStringLiteral("copy_spacing_x"), s.layout.copy_spacing_x);
    lay.insert(QStringLiteral("copy_spacing_y"), s.layout.copy_spacing_y);
    lay.insert(QStringLiteral("auto_scale"), s.layout.auto_scale);
    lay.insert(QStringLiteral("align_center_x"), s.layout.align_center_x);
    lay.insert(QStringLiteral("align_center_y"), s.layout.align_center_y);
    lay.insert(QStringLiteral("auto_copies"), s.layout.auto_copies);
    lay.insert(QStringLiteral("auto_rotate"), s.layout.auto_rotate);
    lay.insert(QStringLiteral("lock_scale"), s.layout.lock_scale);
    lay.insert(QStringLiteral("auto_shift"), s.layout.auto_shift);
    lay.insert(QStringLiteral("layout_offset_x"), s.layout.layout_offset_x);
    lay.insert(QStringLiteral("layout_offset_y"), s.layout.layout_offset_y);
    o.insert(QStringLiteral("layout"), lay);

    QJsonObject weed;
    weed.insert(QStringLiteral("plot"), s.weedlines.plot_weedline);
    weed.insert(QStringLiteral("copy"), s.weedlines.copy_weedline);
    weed.insert(QStringLiteral("plot_pad_l"), s.weedlines.plot_pad_left);
    weed.insert(QStringLiteral("plot_pad_t"), s.weedlines.plot_pad_top);
    weed.insert(QStringLiteral("plot_pad_r"), s.weedlines.plot_pad_right);
    weed.insert(QStringLiteral("plot_pad_b"), s.weedlines.plot_pad_bottom);
    weed.insert(QStringLiteral("copy_pad_l"), s.weedlines.copy_pad_left);
    weed.insert(QStringLiteral("copy_pad_t"), s.weedlines.copy_pad_top);
    weed.insert(QStringLiteral("copy_pad_r"), s.weedlines.copy_pad_right);
    weed.insert(QStringLiteral("copy_pad_b"), s.weedlines.copy_pad_bottom);
    o.insert(QStringLiteral("weedlines"), weed);
    o.insert(QStringLiteral("feed_to_end"), s.feed_to_end);
    o.insert(QStringLiteral("feed_after"), s.feed_after);

    QJsonArray layer_arr;
    for (const LayerFilterEntry& e : s.layer_filters) {
        QJsonObject le;
        le.insert(QStringLiteral("id"), e.layer_id);
        le.insert(QStringLiteral("name"), e.name);
        le.insert(QStringLiteral("enabled"), e.enabled);
        le.insert(QStringLiteral("pass_count"), e.pass_count);
        le.insert(QStringLiteral("offset_x"), e.offset_x);
        le.insert(QStringLiteral("offset_y"), e.offset_y);
        layer_arr.append(le);
    }
    o.insert(QStringLiteral("layer_filters"), layer_arr);

    QJsonArray color_arr;
    for (const ColorFilterEntry& e : s.color_filters) {
        QJsonObject ce;
        ce.insert(QStringLiteral("key"), e.color_key);
        ce.insert(QStringLiteral("is_fill"), e.is_fill);
        ce.insert(QStringLiteral("enabled"), e.enabled);
        ce.insert(QStringLiteral("pass_count"), e.pass_count);
        color_arr.append(ce);
    }
    o.insert(QStringLiteral("color_filters"), color_arr);

    QJsonObject dev;
    dev.insert(QStringLiteral("preset_id"), s.device.preset_id);
    dev.insert(QStringLiteral("transport"), int(s.device.transport));
    dev.insert(QStringLiteral("port"), s.device.port_name);
    dev.insert(QStringLiteral("baud"), int(s.device.baud_rate));
    dev.insert(QStringLiteral("output_path"), s.device.output_path);
    dev.insert(QStringLiteral("printer_name"), s.device.printer_name);
    dev.insert(QStringLiteral("swap_xy"), s.device.swap_xy);
    dev.insert(QStringLiteral("mirror_x"), s.device.mirror_x);
    dev.insert(QStringLiteral("mirror_y"), s.device.mirror_y);
    dev.insert(QStringLiteral("device_scale"), s.device.device_scale);
    dev.insert(QStringLiteral("after_connect"), s.device.after_connect_command);
    dev.insert(QStringLiteral("before_job"), s.device.before_job_command);
    dev.insert(QStringLiteral("after_job"), s.device.after_job_command);
    o.insert(QStringLiteral("device"), dev);

    return o;
}

bool plotJobSettingsFromJson(const QJsonObject& o, PlotJobSettings& s, QString* err)
{
    const QString ord = o.value(QStringLiteral("order")).toString();
    if (!ord.isEmpty()) {
        OrderStrategy os = OrderStrategy::Normal;
        if (!orderStrategyFromCli(QStringView(ord), os)) {
            if (err)
                *err = QStringLiteral("JSON: nieznana strategia order.");
            return false;
        }
        s.order = os;
    }

    const QJsonObject pb = o.value(QStringLiteral("protocol_block")).toObject();
    if (!pb.isEmpty() && !protocolSettingsFromJson(pb, s.protocol, err))
        return false;

    if (o.contains(QStringLiteral("flatten_step")))
        s.flatten_step = o.value(QStringLiteral("flatten_step")).toDouble();
    if (o.contains(QStringLiteral("velocity")))
        s.velocity = o.value(QStringLiteral("velocity")).toInt();
    if (o.contains(QStringLiteral("repeat_steps")))
        s.repeat.steps = o.value(QStringLiteral("repeat_steps")).toInt();
    if (o.contains(QStringLiteral("repeat_gap")))
        s.repeat.closed_loop_distance = o.value(QStringLiteral("repeat_gap")).toDouble();
    if (o.contains(QStringLiteral("min_jump")))
        s.min_line.min_jump = o.value(QStringLiteral("min_jump")).toDouble();
    if (o.contains(QStringLiteral("min_path")))
        s.min_line.min_path = o.value(QStringLiteral("min_path")).toDouble();
    if (o.contains(QStringLiteral("min_edge")))
        s.min_line.min_edge = o.value(QStringLiteral("min_edge")).toDouble();
    if (o.contains(QStringLiteral("min_shift")))
        s.min_line.min_shift = o.value(QStringLiteral("min_shift")).toDouble();
    if (o.contains(QStringLiteral("blade_offset")))
        s.blade.offset = o.value(QStringLiteral("blade_offset")).toDouble();
    if (o.contains(QStringLiteral("blade_cutoff_deg")))
        s.blade.cutoff_deg = o.value(QStringLiteral("blade_cutoff_deg")).toDouble();
    if (o.contains(QStringLiteral("blade_quality")))
        s.blade.quality_factor = o.value(QStringLiteral("blade_quality")).toDouble();
    if (o.contains(QStringLiteral("overcut")))
        s.overcut = o.value(QStringLiteral("overcut")).toDouble();
    if (o.contains(QStringLiteral("closed_poly_eps")))
        s.closed_poly_eps = o.value(QStringLiteral("closed_poly_eps")).toDouble();

    const QJsonObject mat = o.value(QStringLiteral("material")).toObject();
    if (!mat.isEmpty()) {
        s.material.width = mat.value(QStringLiteral("width")).toDouble(s.material.width);
        s.material.height = mat.value(QStringLiteral("height")).toDouble(s.material.height);
        s.material.padding_left = mat.value(QStringLiteral("pad_l")).toDouble(s.material.padding_left);
        s.material.padding_top = mat.value(QStringLiteral("pad_t")).toDouble(s.material.padding_top);
        s.material.padding_right = mat.value(QStringLiteral("pad_r")).toDouble(s.material.padding_right);
        s.material.padding_bottom = mat.value(QStringLiteral("pad_b")).toDouble(s.material.padding_bottom);
        if (mat.contains(QStringLiteral("is_roll")))
            s.material.is_roll = mat.value(QStringLiteral("is_roll")).toBool();
        if (mat.contains(QStringLiteral("cost_per_area")))
            s.material.cost_per_area = mat.value(QStringLiteral("cost_per_area")).toDouble();
        if (mat.contains(QStringLiteral("use_custom_force_speed")))
            s.material.use_custom_force_speed =
                mat.value(QStringLiteral("use_custom_force_speed")).toBool();
        if (mat.contains(QStringLiteral("force")))
            s.material.force = mat.value(QStringLiteral("force")).toInt();
        if (mat.contains(QStringLiteral("speed")))
            s.material.speed = mat.value(QStringLiteral("speed")).toInt();
    }

    if (o.contains(QStringLiteral("plugin_id")))
        s.plugin_id = o.value(QStringLiteral("plugin_id")).toString();

    const QJsonObject lay = o.value(QStringLiteral("layout")).toObject();
    if (!lay.isEmpty()) {
        s.layout.scale_x = lay.value(QStringLiteral("scale_x")).toDouble(s.layout.scale_x);
        s.layout.scale_y = lay.value(QStringLiteral("scale_y")).toDouble(s.layout.scale_y);
        s.layout.mirror_x = lay.value(QStringLiteral("mirror_x")).toBool();
        s.layout.mirror_y = lay.value(QStringLiteral("mirror_y")).toBool();
        s.layout.rotation_deg = lay.value(QStringLiteral("rotation")).toDouble();
        s.layout.copies = lay.value(QStringLiteral("copies")).toInt();
        s.layout.copy_spacing_x = lay.value(QStringLiteral("copy_spacing_x")).toDouble();
        s.layout.copy_spacing_y = lay.value(QStringLiteral("copy_spacing_y")).toDouble();
        s.layout.auto_scale = lay.value(QStringLiteral("auto_scale")).toBool();
        s.layout.align_center_x = lay.value(QStringLiteral("align_center_x")).toBool();
        s.layout.align_center_y = lay.value(QStringLiteral("align_center_y")).toBool();
        if (lay.contains(QStringLiteral("auto_copies")))
            s.layout.auto_copies = lay.value(QStringLiteral("auto_copies")).toBool();
        if (lay.contains(QStringLiteral("lock_scale")))
            s.layout.lock_scale = lay.value(QStringLiteral("lock_scale")).toBool();
        if (lay.contains(QStringLiteral("auto_shift")))
            s.layout.auto_shift = lay.value(QStringLiteral("auto_shift")).toBool();
        if (lay.contains(QStringLiteral("layout_offset_x")))
            s.layout.layout_offset_x = lay.value(QStringLiteral("layout_offset_x")).toDouble();
        if (lay.contains(QStringLiteral("layout_offset_y")))
            s.layout.layout_offset_y = lay.value(QStringLiteral("layout_offset_y")).toDouble();
        if (lay.contains(QStringLiteral("auto_rotate")))
            s.layout.auto_rotate = lay.value(QStringLiteral("auto_rotate")).toBool();
    }

    const QJsonObject weed = o.value(QStringLiteral("weedlines")).toObject();
    if (!weed.isEmpty()) {
        s.weedlines.plot_weedline = weed.value(QStringLiteral("plot")).toBool();
        s.weedlines.copy_weedline = weed.value(QStringLiteral("copy")).toBool();
        s.weedlines.plot_pad_left = weed.value(QStringLiteral("plot_pad_l")).toDouble(10);
        s.weedlines.plot_pad_top = weed.value(QStringLiteral("plot_pad_t")).toDouble(10);
        s.weedlines.plot_pad_right = weed.value(QStringLiteral("plot_pad_r")).toDouble(10);
        s.weedlines.plot_pad_bottom = weed.value(QStringLiteral("plot_pad_b")).toDouble(10);
        s.weedlines.copy_pad_left = weed.value(QStringLiteral("copy_pad_l")).toDouble(10);
        s.weedlines.copy_pad_top = weed.value(QStringLiteral("copy_pad_t")).toDouble(10);
        s.weedlines.copy_pad_right = weed.value(QStringLiteral("copy_pad_r")).toDouble(10);
        s.weedlines.copy_pad_bottom = weed.value(QStringLiteral("copy_pad_b")).toDouble(10);
    }

    if (o.contains(QStringLiteral("feed_to_end")))
        s.feed_to_end = o.value(QStringLiteral("feed_to_end")).toBool();
    if (o.contains(QStringLiteral("feed_after")))
        s.feed_after = o.value(QStringLiteral("feed_after")).toDouble();

    s.layer_filters.clear();
    const QJsonArray layer_arr = o.value(QStringLiteral("layer_filters")).toArray();
    for (const QJsonValue& v : layer_arr) {
        const QJsonObject le = v.toObject();
        LayerFilterEntry e;
        e.layer_id = le.value(QStringLiteral("id")).toString();
        e.name = le.value(QStringLiteral("name")).toString();
        e.enabled = le.value(QStringLiteral("enabled")).toBool(true);
        e.pass_count = std::max(1, le.value(QStringLiteral("pass_count")).toInt(1));
        e.offset_x = le.value(QStringLiteral("offset_x")).toDouble();
        e.offset_y = le.value(QStringLiteral("offset_y")).toDouble();
        s.layer_filters.push_back(e);
    }

    s.color_filters.clear();
    const QJsonArray color_arr = o.value(QStringLiteral("color_filters")).toArray();
    for (const QJsonValue& v : color_arr) {
        const QJsonObject ce = v.toObject();
        ColorFilterEntry e;
        e.color_key = ce.value(QStringLiteral("key")).toString();
        e.is_fill = ce.value(QStringLiteral("is_fill")).toBool(true);
        e.enabled = ce.value(QStringLiteral("enabled")).toBool(true);
        e.pass_count = std::max(1, ce.value(QStringLiteral("pass_count")).toInt(1));
        s.color_filters.push_back(e);
    }

    const QJsonObject dev = o.value(QStringLiteral("device")).toObject();
    if (!dev.isEmpty()) {
        s.device.preset_id = dev.value(QStringLiteral("preset_id")).toString();
        s.device.transport =
            static_cast<PlotTransportKind>(dev.value(QStringLiteral("transport")).toInt());
        s.device.port_name = dev.value(QStringLiteral("port")).toString(s.device.port_name);
        s.device.baud_rate = dev.value(QStringLiteral("baud")).toInt(s.device.baud_rate);
        s.device.output_path = dev.value(QStringLiteral("output_path")).toString();
        s.device.printer_name = dev.value(QStringLiteral("printer_name")).toString();
        s.device.swap_xy = dev.value(QStringLiteral("swap_xy")).toBool();
        s.device.mirror_x = dev.value(QStringLiteral("mirror_x")).toBool();
        s.device.mirror_y = dev.value(QStringLiteral("mirror_y")).toBool();
        s.device.device_scale = dev.value(QStringLiteral("device_scale")).toDouble(1.0);
        if (dev.contains(QStringLiteral("after_connect")))
            s.device.after_connect_command = dev.value(QStringLiteral("after_connect")).toString();
        if (dev.contains(QStringLiteral("before_job")))
            s.device.before_job_command = dev.value(QStringLiteral("before_job")).toString();
        if (dev.contains(QStringLiteral("after_job")))
            s.device.after_job_command = dev.value(QStringLiteral("after_job")).toString();
    }

    return true;
}

} // namespace

QString plotJobSettingsToJsonString(const PlotJobSettings& settings)
{
    const QJsonDocument doc(plotJobSettingsToJson(settings));
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool plotJobSettingsFromJsonString(const QByteArray& utf8_json, PlotJobSettings& settings_out,
                                   QString* error_message)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(utf8_json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error_message)
            *error_message = QStringLiteral("JSON: błąd składni ustawień.");
        return false;
    }
    return plotJobSettingsFromJson(doc.object(), settings_out, error_message);
}

QString exportJobDocumentToJson(const QString& source_path, const QString& source_kind,
                                const QRectF& bounds, const PlotJobSettings& settings)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("inkcut-job"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("source_path"), source_path);
    root.insert(QStringLiteral("source_kind"), source_kind);

    QJsonObject bb;
    bb.insert(QStringLiteral("x"), bounds.x());
    bb.insert(QStringLiteral("y"), bounds.y());
    bb.insert(QStringLiteral("width"), bounds.width());
    bb.insert(QStringLiteral("height"), bounds.height());
    root.insert(QStringLiteral("bounds"), bb);

    root.insert(QStringLiteral("settings"), plotJobSettingsToJson(settings));

    const QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool importJobDocumentJson(const QByteArray& utf8_json, PlotJobSettings& settings_out,
                           QString* source_path_out, QString* source_kind_out, QRectF* bounds_out,
                           QString* error_message)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(utf8_json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error_message)
            *error_message = QStringLiteral("JSON: błąd składni.");
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("inkcut-job")) {
        if (error_message)
            *error_message = QStringLiteral("JSON: oczekiwano format=inkcut-job.");
        return false;
    }

    if (source_path_out)
        *source_path_out = root.value(QStringLiteral("source_path")).toString();
    if (source_kind_out)
        *source_kind_out = root.value(QStringLiteral("source_kind")).toString();

    const QJsonObject bb = root.value(QStringLiteral("bounds")).toObject();
    if (bounds_out && !bb.isEmpty()) {
        bounds_out->setX(bb.value(QStringLiteral("x")).toDouble());
        bounds_out->setY(bb.value(QStringLiteral("y")).toDouble());
        bounds_out->setWidth(bb.value(QStringLiteral("width")).toDouble());
        bounds_out->setHeight(bb.value(QStringLiteral("height")).toDouble());
    }

    const QJsonObject st = root.value(QStringLiteral("settings")).toObject();
    if (st.isEmpty()) {
        if (error_message)
            *error_message = QStringLiteral("JSON: brak settings.");
        return false;
    }

    return plotJobSettingsFromJson(st, settings_out, error_message);
}

} // namespace inkcut
