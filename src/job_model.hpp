// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "filters.hpp"
#include "ordering.hpp"
#include "protocols.hpp"

#include <QPointF>
#include <QString>
#include <QVector>

namespace inkcut {

struct MaterialSettings {
    double width = 600;
    double height = 400;
    double padding_left = 10;
    double padding_top = 10;
    double padding_right = 10;
    double padding_bottom = 10;
    bool is_roll = false;
    double cost_per_area = 0;
    /// Gdy true, wysyłane są FS/VS (plotery) lub F w G-code (GRBL).
    bool use_custom_force_speed = false;
    int force = 10;
    /// Prędkość cięcia (cm/s) — HPGL VS i pokrewne.
    int speed = 120;
    int gcode_feed_cut_mm_min = 0;
    int gcode_feed_rapid_mm_min = 0;
};

struct GraphicLayoutSettings {
    double scale_x = 1.0;
    double scale_y = 1.0;
    bool lock_scale = true;
    bool mirror_x = false;
    bool mirror_y = false;
    double rotation_deg = 0;
    bool auto_rotate = false;
    bool align_center_x = false;
    bool align_center_y = false;
    bool auto_scale = false;
    bool auto_shift = true;
    double layout_offset_x = 0;
    double layout_offset_y = 0;
    int copies = 1;
    double copy_spacing_x = 10;
    double copy_spacing_y = 10;
    bool auto_copies = false;
    int stack_size_x = 0;
    int stack_size_y = 0;
};

struct WeedlineSettings {
    bool plot_weedline = false;
    double plot_pad_left = 10;
    double plot_pad_top = 10;
    double plot_pad_right = 10;
    double plot_pad_bottom = 10;
    bool copy_weedline = false;
    double copy_pad_left = 10;
    double copy_pad_top = 10;
    double copy_pad_right = 10;
    double copy_pad_bottom = 10;
};

struct LayerFilterEntry {
    QString layer_id;
    QString name;
    bool enabled = true;
    /// Liczba przejść plotera dla tej warstwy (twardy materiał).
    int pass_count = 1;
    double offset_x = 0;
    double offset_y = 0;
};

struct ColorFilterEntry {
    QString color_key;
    bool is_fill = true;
    bool enabled = true;
    int pass_count = 1;
};

enum class PlotTransportKind { SerialPort, TcpIp, FileOutput, Printer };

struct DeviceSetup {
    QString name;
    bool custom = false;
    QString preset_id;
    QString manufacturer;
    QString model_name;
    PlotTransportKind transport = PlotTransportKind::SerialPort;
    QString port_name = QStringLiteral("/dev/ttyUSB0");
    qint32 baud_rate = 115200;
    QString tcp_host = QStringLiteral("127.0.0.1");
    int tcp_port = 23;
    /// 5, 6, 7 lub 8 (QSerialPort::DataBits).
    int data_bits = 8;
    /// 0=None, 1=Even, 2=Odd, 3=Space, 4=Mark.
    int parity = 0;
    /// 1 lub 2 stop bits.
    int stop_bits = 1;
    bool flow_rts_cts = false;
    bool flow_dsr_dtr = false;
    bool flow_xon_xoff = false;
    QString output_path;
    QString printer_name;
    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    double device_scale = 1.0;
    /// Przed/po połączeniu oraz przed/po jobie (HPGL lub G-code, linie rozdzielone \\n).
    QString before_connect_command;
    QString after_connect_command;
    QString before_job_command;
    QString after_job_command;
};

struct PlotJobSettings {
    OrderStrategy order = OrderStrategy::Normal;
    ProtocolSettings protocol;
    double flatten_step = 1.0;
    int velocity = 120;
    RepeatFilterConfig repeat;
    MinLineFilterConfig min_line;
    BladeOffsetConfig blade;
    double overcut = 0;
    double closed_poly_eps = 0.25;

    MaterialSettings material;
    GraphicLayoutSettings layout;
    WeedlineSettings weedlines;
    QVector<LayerFilterEntry> layer_filters;
    QVector<ColorFilterEntry> color_filters;
    DeviceSetup device;
    /// Id wtyczki urządzenia (`DevicePlugin::pluginId()`), puste = ręczny port.
    QString plugin_id;

    /// Podawanie materiału po jobie (jak `Job.feed_to_end` / `feed_after` w Pythonie).
    bool feed_to_end = false;
    double feed_after = 0;
};

} // namespace inkcut
