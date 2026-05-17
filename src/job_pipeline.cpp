// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_pipeline.hpp"

#include "device_plugin.hpp"
#include "filters.hpp"
#include "job_layout.hpp"
#include "ordering.hpp"
#include "svg_document.hpp"
#include "svg_layers.hpp"
#include "svg_path.hpp"

#include <QStringView>
#include <algorithm>
#include <sstream>

#include "i18n.hpp"

namespace inkcut {

namespace {

QString layerXmlForEntry(const QString& xml, const LayerFilterEntry& layer,
                         const QVector<LayerFilterEntry>& all_layers,
                         const QVector<LayerFilterEntry>& discovered)
{
    if (layer.layer_id.isEmpty()) {
        if (!discovered.isEmpty())
            return {};
        return filterSvgXmlByLayers(xml, all_layers);
    }
    return filterSvgXmlOnlyLayer(xml, layer.layer_id);
}

bool appendPathWithPasses(QPainterPath& dest, const QString& xml, const QString& document_base_dir,
                          int pass_count, const RepeatFilterConfig& repeat_base,
                          QString* error_message, QStringList* warnings)
{
    QPainterPath chunk;
    QString err;
    if (!loadSvgPainterPath(xml, chunk, &err, document_base_dir, warnings)) {
        if (error_message && !err.isEmpty())
            *error_message = err;
        return false;
    }
    if (chunk.isEmpty())
        return false;

    RepeatFilterConfig rc = repeat_base;
    rc.steps = std::max(1, pass_count);
    if (rc.steps > 1)
        chunk = applyRepeatFilter(chunk, rc);
    dest.addPath(chunk);
    return true;
}

QVector<LayerFilterEntry> enabledLayersForLoad(const PlotJobSettings& settings,
                                               const QVector<LayerFilterEntry>& discovered)
{
    QVector<LayerFilterEntry> rows = settings.layer_filters;
    if (rows.isEmpty() && !discovered.isEmpty())
        rows = discovered;

    QVector<LayerFilterEntry> enabled;
    if (rows.isEmpty()) {
        LayerFilterEntry whole;
        whole.layer_id.clear();
        whole.name = trInk("Cały dokument");
        whole.enabled = true;
        whole.pass_count = 1;
        for (const LayerFilterEntry& e : settings.layer_filters) {
            if (e.layer_id.isEmpty()) {
                whole.enabled = e.enabled;
                whole.pass_count = std::max(1, e.pass_count);
            }
        }
        enabled.push_back(whole);
        return enabled;
    }

    for (const LayerFilterEntry& e : rows) {
        if (e.enabled)
            enabled.push_back(e);
    }
    return enabled;
}

} // namespace

int maxEnabledLayerPassCount(const QVector<LayerFilterEntry>& layers)
{
    int n = 1;
    for (const LayerFilterEntry& e : layers) {
        if (e.enabled)
            n = std::max(n, e.pass_count);
    }
    return n;
}

int maxEnabledColorPassCount(const QVector<ColorFilterEntry>& colors)
{
    int n = 1;
    for (const ColorFilterEntry& e : colors) {
        if (e.enabled)
            n = std::max(n, e.pass_count);
    }
    return n;
}

bool loadSvgDesignPath(const QString& xml, const QString& document_base_dir,
                       const PlotJobSettings& settings, QPainterPath& out,
                       QString* error_message, QStringList* warnings)
{
    const QVector<LayerFilterEntry> discovered = discoverSvgLayers(xml);
    const QVector<LayerFilterEntry> enabled_layers = enabledLayersForLoad(settings, discovered);

    QVector<ColorFilterEntry> enabled_colors;
    enabled_colors.reserve(settings.color_filters.size());
    for (const ColorFilterEntry& c : settings.color_filters) {
        if (c.enabled)
            enabled_colors.push_back(c);
    }

    QPainterPath combined;
    bool any = false;

    for (const LayerFilterEntry& layer : enabled_layers) {
        const QString layer_xml = layerXmlForEntry(xml, layer, settings.layer_filters, discovered);
        if (layer_xml.isEmpty())
            continue;

        QPainterPath layer_path;

        if (enabled_colors.isEmpty()) {
            if (!appendPathWithPasses(layer_path, layer_xml, document_base_dir, layer.pass_count,
                                      settings.repeat, error_message, warnings))
                continue;
        } else {
            for (const ColorFilterEntry& color : enabled_colors) {
                const QString color_xml =
                    filterSvgXmlKeepOnlyColor(layer_xml, color.color_key, color.is_fill);
                appendPathWithPasses(layer_path, color_xml, document_base_dir, color.pass_count,
                                     settings.repeat, error_message, warnings);
            }
            if (layer.pass_count > 1 && !layer_path.isEmpty()) {
                RepeatFilterConfig rc = settings.repeat;
                rc.steps = std::max(1, layer.pass_count);
                layer_path = applyRepeatFilter(layer_path, rc);
            }
        }

        if (layer_path.isEmpty())
            continue;

        if (layer.offset_x != 0.0 || layer.offset_y != 0.0)
            layer_path =
                QTransform::fromTranslate(layer.offset_x, layer.offset_y).map(layer_path);

        combined.addPath(layer_path);
        any = true;
    }

    if (!any) {
        if (error_message)
            *error_message = trInk("Brak geometrii w włączonych warstwach/kolorach.");
        return false;
    }

    out = combined;
    return true;
}

static bool polyIsClosed(const std::vector<QPointF>& poly, double eps)
{
    if (poly.size() < 2)
        return false;
    const QPointF d = poly.front() - poly.back();
    return QPointF::dotProduct(d, d) <= eps * eps;
}

QPainterPath processFilledTracePath(const QPainterPath& raw_path, const PlotJobSettings& settings,
                                    DevicePlugin* device_plugin)
{
    QPainterPath p = applyJobLayout(raw_path, settings);
    if (device_plugin)
        p = device_plugin->transformPath(p);
    p = applyDeviceOutputTransform(p, settings.device);
    return p;
}

QPainterPath processJobPath(const QPainterPath& raw_path, const PlotJobSettings& settings,
                            DevicePlugin* device_plugin)
{
    QPainterPath p = applyCutOrder(raw_path, settings.order);
    p = applyJobLayout(p, settings);
    p = applyRepeatFilter(p, settings.repeat);
    p = applyMinLineFilter(p, settings.min_line);
    p = applyBladeOffsetFilter(p, settings.blade);
    if (device_plugin)
        p = device_plugin->transformPath(p);
    p = applyDeviceOutputTransform(p, settings.device);
    return p;
}

std::string buildPlotProgram(const QPainterPath& raw_path, const PlotJobSettings& settings,
                             DevicePlugin* device_plugin, bool filled_silhouette)
{
    const QPainterPath job_path =
        filled_silhouette ? processFilledTracePath(raw_path, settings, device_plugin)
                          : processJobPath(raw_path, settings, device_plugin);
    auto polylines = path_to_polylines(job_path, settings.flatten_step);

    std::string acc;
    ProtocolSettings plot_proto = settings.protocol;
    if (settings.material.use_custom_force_speed) {
        if (plot_proto.protocol == PlotProtocol::GCode) {
            plot_proto.gcode.feed_mm_min = settings.material.gcode_feed_cut_mm_min;
            plot_proto.gcode.feed_rapid_mm_min = settings.material.gcode_feed_rapid_mm_min;
        }
    } else if (plot_proto.protocol == PlotProtocol::GCode) {
        plot_proto.gcode.feed_mm_min = 0;
        plot_proto.gcode.feed_rapid_mm_min = 0;
    }

    PlotStreamEncoder enc([&](std::string chunk) { acc += std::move(chunk); }, plot_proto);

    enc.send_command_block(settings.device.before_connect_command);
    enc.connection_made();
    enc.send_command_block(settings.device.after_connect_command);
    if (settings.material.use_custom_force_speed && plot_proto.protocol != PlotProtocol::GCode) {
        enc.set_force(settings.material.force);
        if (settings.material.speed > 0)
            enc.set_velocity(settings.material.speed);
    }

    enc.send_command_block(settings.device.before_job_command);

    for (auto poly : polylines) {
        if (poly.size() < 2)
            continue;
        if (settings.overcut > 0 && polyIsClosed(poly, settings.closed_poly_eps))
            applyOvercutToClosedPolyline(poly, settings.overcut);
        append_polyline_plot(enc, poly);
    }
    enc.send_command_block(settings.device.after_job_command);
    enc.finish();
    return acc;
}

PlotProtocol plotProtocolFromCli(QStringView name)
{
    if (name.compare(QLatin1String("gcode"), Qt::CaseInsensitive) == 0)
        return PlotProtocol::GCode;
    return PlotProtocol::GCode;
}

QString plotProtocolToCli(PlotProtocol p)
{
    Q_UNUSED(p);
    return QStringLiteral("gcode");
}

QString orderStrategyToCli(OrderStrategy o)
{
    switch (o) {
    case OrderStrategy::Normal:
        return QStringLiteral("normal");
    case OrderStrategy::Reversed:
        return QStringLiteral("reversed");
    case OrderStrategy::MinX:
        return QStringLiteral("min-x");
    case OrderStrategy::MaxX:
        return QStringLiteral("max-x");
    case OrderStrategy::MinY:
        return QStringLiteral("min-y");
    case OrderStrategy::MaxY:
        return QStringLiteral("max-y");
    case OrderStrategy::ShortestPath:
        return QStringLiteral("shortest");
    case OrderStrategy::Hilbert:
        return QStringLiteral("hilbert");
    case OrderStrategy::ZCurve:
        return QStringLiteral("zcurve");
    }
    return QStringLiteral("normal");
}

bool orderStrategyFromCli(QStringView name, OrderStrategy& out)
{
    if (name.compare(QLatin1String("normal"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::Normal;
        return true;
    }
    if (name.compare(QLatin1String("reversed"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::Reversed;
        return true;
    }
    if (name.compare(QLatin1String("min-x"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::MinX;
        return true;
    }
    if (name.compare(QLatin1String("max-x"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::MaxX;
        return true;
    }
    if (name.compare(QLatin1String("min-y"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::MinY;
        return true;
    }
    if (name.compare(QLatin1String("max-y"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::MaxY;
        return true;
    }
    if (name.compare(QLatin1String("shortest"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::ShortestPath;
        return true;
    }
    if (name.compare(QLatin1String("hilbert"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::Hilbert;
        return true;
    }
    if (name.compare(QLatin1String("zcurve"), Qt::CaseInsensitive) == 0) {
        out = OrderStrategy::ZCurve;
        return true;
    }
    return false;
}

} // namespace inkcut
