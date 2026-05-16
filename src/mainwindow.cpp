// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.hpp"

#include "dxf_document.hpp"
#include "device_presets.hpp"
#include "device_setup_dialog.hpp"
#include "filters.hpp"
#include "job_export.hpp"
#include "bitmap_trace.hpp"
#include "bitmap_trace_dialog.hpp"
#include "dxf_to_svg.hpp"
#include "i18n.hpp"
#include "app_settings.hpp"
#include "app_theme.hpp"
#include "app_units.hpp"
#include "plot_live_position.hpp"
#include "settings_dialog.hpp"
#include "job_layout.hpp"
#include "job_pipeline.hpp"
#include "job_stack.hpp"
#include "ordering.hpp"
#include "path_utils.hpp"
#include "job_history.hpp"
#include "live_plot_view.hpp"
#include "preview_plot_view.hpp"
#include "job_layout.hpp"
#include "ui_icons.hpp"
#include "ui_profile.hpp"
#include "plot_sender.hpp"
#include "plot_transport.hpp"
#include "plugin_loader.hpp"
#include "protocols.hpp"
#include "serial_sender.hpp"
#include "svg_document.hpp"
#include "svg_layers.hpp"
#include "svg_path.hpp"

#include "device_plugin.hpp"

#include <QApplication>
#include <QDialog>
#include <QProgressDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QThread>
#include <QUuid>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPen>
#include <QPlainTextEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStyle>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSerialPort>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QDateTime>
#include <QGraphicsView>
#include <QGridLayout>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QTimer>
#include <QHeaderView>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <QPainterPath>

namespace inkcut {

namespace {

QString readFileUtf8(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

QString format_monitor_chunk(const QByteArray& b, bool hex)
{
    if (hex)
        return QString::fromLatin1(b.toHex(' '));
    QString s;
    s.reserve(int(b.size()));
    for (unsigned char uc : b) {
        const char c = char(uc);
        if (c >= 32 && c < 127)
            s += QLatin1Char(c);
        else
            s += QStringLiteral("\\x%1").arg(uc, 2, 16, QLatin1Char('0'));
    }
    return s;
}

void styleToolButton(QPushButton* btn, const QIcon& icon, const QString& tooltip, bool icon_only = true)
{
    if (!btn)
        return;
    btn->setIcon(icon);
    btn->setIconSize(QSize(22, 22));
    btn->setToolTip(tooltip);
    if (icon_only)
        btn->setText({});
}

QPen penPlotMove()
{
    return QPen(QColor(0, 130, 215, 185), 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QPen penPlotCut()
{
    return QPen(QColor(70, 70, 70), 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QString colorFilterStorageKey(const QString& color_key, bool is_fill)
{
    return color_key + (is_fill ? QStringLiteral("|f") : QStringLiteral("|s"));
}

QWidget* attachPlotZoomBar(QWidget* parent, LivePlotView* view)
{
    auto* bar = new QWidget(parent);
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(8);

    auto make_btn = [&](const QString& label, const QString& tip, void (LivePlotView::*slot)()) {
        auto* b = new QPushButton(label, bar);
        b->setObjectName(QStringLiteral("plot_zoom_btn"));
        b->setToolTip(tip);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QObject::connect(b, &QPushButton::clicked, view, slot);
        lay->addWidget(b);
    };

    make_btn(QStringLiteral("−"), trInk("Pomniejsz widok (kółko myszy też działa)"),
             &LivePlotView::zoomOut);
    make_btn(QStringLiteral("+"), trInk("Powiększ widok (kółko myszy też działa)"),
             &LivePlotView::zoomIn);
    make_btn(QStringLiteral("⤢"), trInk("Dopasuj cały rysunek do okna"), &LivePlotView::fitAll);
    lay->addStretch(1);
    return bar;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Inkcut C++"));
    setDockNestingEnabled(true);

    QMenuBar* mb = menuBar();
    file_menu_ = mb->addMenu(trInk("Plik"));
    open_svg_action_ = file_menu_->addAction(trInk("Otwórz SVG / DXF…"), this, &MainWindow::onOpenSvg);
    import_bitmap_action_ =
        file_menu_->addAction(trInk("Import bitmapy…"), this, &MainWindow::onImportBitmap);
    recent_menu_ = file_menu_->addMenu(trInk("Ostatnie pliki"));
    rebuildRecentMenu();
    file_menu_->addSeparator();
    file_menu_->addAction(trInk("Zapisz program plotera…"), this, &MainWindow::onExportProgram);
    file_menu_->addAction(trInk("Eksport zadania (JSON)…"), this, &MainWindow::onExportJobJson);
    file_menu_->addSeparator();
    QAction* send_action = file_menu_->addAction(trInk("Wyślij na urządzenie…"), this,
                                                &MainWindow::onSend);
    send_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));

    device_menu_ = mb->addMenu(trInk("Urządzenie"));
    device_menu_->addAction(trInk("Konfiguracja urządzenia…"), this,
                           &MainWindow::onOpenDeviceSetup);
    device_menu_->addSeparator();
    QAction* device_send = device_menu_->addAction(trInk("Wyślij na urządzenie…"), this,
                                                  &MainWindow::onSend);
    device_send->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));
    device_menu_->addSeparator();
    device_menu_->addAction(trInk("Odśwież wtyczki"), this,
                           &MainWindow::refreshDevicePlugins);

    settings_menu_ = mb->addMenu(trInk("Ustawienia"));
    settings_menu_->addAction(trInk("Ustawienia…"), this, &MainWindow::onOpenSettings);
    help_menu_ = mb->addMenu(trInk("Pomoc"));
    help_menu_->addAction(trInk("O programie…"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("Inkcut C++"),
                           trInk("Port C++/Qt aplikacji Inkcut.\nWersja 0.2"));
    });

    scene_ = new QGraphicsScene(this);
    preview_view_ = new PreviewPlotView(this);
    preview_view_->setScene(scene_);
    preview_view_->setAxisUnitMm(1.0);
    preview_view_->setBackgroundBrush(QColor(250, 250, 250));
    connect(preview_view_, &PreviewPlotView::graphicOffsetChanged, this,
            &MainWindow::onPreviewGraphicOffsetChanged);
    connect(preview_view_, &PreviewPlotView::graphicDragFinished, this,
            &MainWindow::onPreviewGraphicDragFinished);

    auto* preview_wrap = new QWidget(this);
    auto* preview_layout = new QVBoxLayout(preview_wrap);
    preview_layout->setContentsMargins(4, 4, 4, 4);
    auto* preview_title = new QLabel(trInk("Podgląd (Preview)"), preview_wrap);
    preview_title->setToolTip(
        trInk("Czerwona przerywana — płaszczyzna urządzenia (x-y).\n"
              "Czarna ciągła — materiał.\n"
              "Czarna przerywana — dostępny obszar (po marginesach).\n"
              "Niebieski — ruch jałowy (move); szary — cięcie (cut)."));
    preview_title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    preview_layout->addWidget(preview_title);
    preview_layout->addWidget(attachPlotZoomBar(preview_wrap, preview_view_));
    preview_layout->addWidget(preview_view_, 1);
    setCentralWidget(preview_wrap);

    left_tabs_ = new QTabWidget(this);
    left_tabs_->setTabPosition(QTabWidget::West);
    left_tabs_->setDocumentMode(true);

    auto* material_tab = new QWidget(left_tabs_);
    auto* lv = new QVBoxLayout(material_tab);
    lv->setSpacing(6);

    auto make_pad_spin = [material_tab]() {
        auto* s = new QDoubleSpinBox(material_tab);
        s->setRange(0, 500);
        s->setDecimals(2);
        s->setSingleStep(0.1);
        s->setSuffix(QStringLiteral(" mm"));
        return s;
    };

    lv->addWidget(new QLabel(trInk("Obszar plotowania"), material_tab));
    mat_w_spin_ = make_pad_spin();
    mat_h_spin_ = make_pad_spin();
    mat_w_spin_->setRange(0.1, 99999.9);
    mat_h_spin_->setRange(0.1, 99999.9);
    mat_w_spin_->setValue(600);
    mat_h_spin_->setValue(400);
    auto* area_row = new QHBoxLayout();
    area_row->setContentsMargins(0, 0, 0, 0);
    area_row->addWidget(new QLabel(trInk("Szer."), material_tab));
    area_row->addWidget(mat_w_spin_, 1);
    area_row->addWidget(new QLabel(trInk("Wys."), material_tab));
    area_row->addWidget(mat_h_spin_, 1);
    lv->addLayout(area_row);

    lv->addWidget(new QLabel(trInk("Marginesy plotowania"), material_tab));
    auto* margins_grid = new QGridLayout();
    margins_grid->setContentsMargins(0, 0, 0, 0);
    mat_pad_l_spin_ = make_pad_spin();
    mat_pad_t_spin_ = make_pad_spin();
    mat_pad_r_spin_ = make_pad_spin();
    mat_pad_b_spin_ = make_pad_spin();
    for (QDoubleSpinBox* s : {mat_pad_l_spin_, mat_pad_t_spin_, mat_pad_r_spin_, mat_pad_b_spin_})
        s->setRange(0, 99999.9);
    mat_pad_l_spin_->setValue(10);
    mat_pad_t_spin_->setValue(10);
    mat_pad_r_spin_->setValue(10);
    mat_pad_b_spin_->setValue(10);
    margins_grid->addWidget(new QLabel(trInk("Lewy"), material_tab), 0, 0);
    margins_grid->addWidget(mat_pad_l_spin_, 0, 1);
    margins_grid->addWidget(new QLabel(trInk("Górny"), material_tab), 0, 2);
    margins_grid->addWidget(mat_pad_t_spin_, 0, 3);
    margins_grid->addWidget(new QLabel(trInk("Prawy"), material_tab), 1, 0);
    margins_grid->addWidget(mat_pad_r_spin_, 1, 1);
    margins_grid->addWidget(new QLabel(trInk("Dolny"), material_tab), 1, 2);
    margins_grid->addWidget(mat_pad_b_spin_, 1, 3);
    lv->addLayout(margins_grid);

    lv->addWidget(new QLabel(trInk("Wyrównanie plotowania"), material_tab));
    auto_shift_chk_ = new QCheckBox(trInk("Przesuń do początku"), material_tab);
    auto_shift_chk_->setChecked(true);
    align_center_x_chk_ = new QCheckBox(trInk("Wyśrodkuj poziomo"), material_tab);
    align_center_y_chk_ = new QCheckBox(trInk("Wyśrodkuj pionowo"), material_tab);
    lv->addWidget(auto_shift_chk_);
    lv->addWidget(align_center_x_chk_);
    lv->addWidget(align_center_y_chk_);

    mat_roll_chk_ = new QCheckBox(trInk("Materiał na rolce"), material_tab);
    lv->addWidget(mat_roll_chk_);
    mat_force_speed_chk_ =
        new QCheckBox(trInk("Własna siła / prędkość (HPGL FS/VS)"), material_tab);
    auto* force_row = new QHBoxLayout();
    mat_force_spin_ = new QSpinBox(material_tab);
    mat_force_spin_->setRange(1, 999);
    mat_force_spin_->setValue(10);
    mat_speed_spin_ = new QSpinBox(material_tab);
    mat_speed_spin_->setRange(1, 99999);
    mat_speed_spin_->setValue(10);
    force_row->addWidget(new QLabel(trInk("Siła"), material_tab));
    force_row->addWidget(mat_force_spin_);
    force_row->addWidget(new QLabel(trInk("Prędk."), material_tab));
    force_row->addWidget(mat_speed_spin_);
  lv->addWidget(mat_force_speed_chk_);
    lv->addLayout(force_row);
    connect(mat_force_speed_chk_, &QCheckBox::toggled, mat_force_spin_, &QWidget::setEnabled);
    connect(mat_force_speed_chk_, &QCheckBox::toggled, mat_speed_spin_, &QWidget::setEnabled);
    mat_force_spin_->setEnabled(false);
    mat_speed_spin_->setEnabled(false);
    connect(mat_roll_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(mat_force_speed_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(mat_force_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onLayoutChanged);
    connect(mat_speed_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onLayoutChanged);

    lv->addWidget(new QLabel(trInk("Podawanie materiału"), material_tab));
    feed_return_rb_ = new QRadioButton(trInk("Powrót do początku"), material_tab);
    feed_after_rb_ = new QRadioButton(trInk("Podaj po"), material_tab);
    feed_return_rb_->setChecked(true);
    auto* feed_grp = new QButtonGroup(material_tab);
    feed_grp->addButton(feed_return_rb_);
    feed_grp->addButton(feed_after_rb_);
    feed_after_spin_ = make_pad_spin();
    feed_after_spin_->setRange(0, 99999.9);
    feed_after_spin_->setValue(0);
    feed_after_spin_->setEnabled(false);
    lv->addWidget(feed_return_rb_);
    auto* feed_after_row = new QHBoxLayout();
    feed_after_row->setContentsMargins(0, 0, 0, 0);
    feed_after_row->addWidget(feed_after_rb_);
    feed_after_row->addWidget(feed_after_spin_, 1);
    lv->addLayout(feed_after_row);
    connect(feed_after_rb_, &QRadioButton::toggled, feed_after_spin_, &QWidget::setEnabled);

    lv->addStretch(1);
    left_tabs_->addTab(material_tab, trInk("Materiał"));

    for (QDoubleSpinBox* s :
         {mat_w_spin_, mat_h_spin_, mat_pad_l_spin_, mat_pad_t_spin_, mat_pad_r_spin_,
          mat_pad_b_spin_, feed_after_spin_}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onLayoutChanged);
    }
    connect(auto_shift_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(align_center_x_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(align_center_y_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(feed_return_rb_, &QRadioButton::toggled, this, &MainWindow::onLayoutChanged);
    connect(feed_after_rb_, &QRadioButton::toggled, this, &MainWindow::onLayoutChanged);

    scale_x_spin_ = new QDoubleSpinBox(this);
    scale_y_spin_ = new QDoubleSpinBox(this);
    scale_x_spin_->hide();
    scale_y_spin_->hide();

    auto* graphic_tab = new QWidget(left_tabs_);
    auto* gv = new QVBoxLayout(graphic_tab);
    gv->setSpacing(6);
    file_path_label_ = new QLabel(trInk("Brak wczytanego pliku."), graphic_tab);
    file_path_label_->setWordWrap(true);
    gv->addWidget(file_path_label_);

    auto* size_gb = new QGroupBox(trInk("Rozmiar grafiki"), graphic_tab);
    auto* size_form = new QFormLayout(size_gb);
    graphic_size_w_label_ = new QLabel(QStringLiteral("—"), size_gb);
    graphic_size_h_label_ = new QLabel(QStringLiteral("—"), size_gb);
    scale_pct_x_spin_ = new QDoubleSpinBox(size_gb);
    scale_pct_y_spin_ = new QDoubleSpinBox(size_gb);
    scale_pct_x_spin_->setRange(0.01, 99999);
    scale_pct_y_spin_->setRange(0.01, 99999);
    scale_pct_x_spin_->setSuffix(QStringLiteral(" %"));
    scale_pct_y_spin_->setSuffix(QStringLiteral(" %"));
    scale_pct_x_spin_->setValue(100);
    scale_pct_y_spin_->setValue(100);
    size_form->addRow(trInk("Szerokość"), graphic_size_w_label_);
    size_form->addRow(trInk("Wysokość"), graphic_size_h_label_);
    size_form->addRow(trInk("Skala X"), scale_pct_x_spin_);
    size_form->addRow(trInk("Skala Y"), scale_pct_y_spin_);
    lock_scale_chk_ = new QCheckBox(trInk("Zablokuj proporcje"), size_gb);
    lock_scale_chk_->setChecked(true);
    size_form->addRow(lock_scale_chk_);
    gv->addWidget(size_gb);

    auto* copies_gb = new QGroupBox(trInk("Kopie grafiki"), graphic_tab);
    auto* copies_v = new QVBoxLayout(copies_gb);
    copies_spin_ = new QSpinBox(copies_gb);
    copies_spin_->setRange(1, 99999);
    copies_spin_->setValue(1);
    auto* copies_row = new QHBoxLayout();
    copies_row->addWidget(copies_spin_, 1);
    add_stack_btn_ = new QPushButton(copies_gb);
    remove_stack_btn_ = new QPushButton(copies_gb);
    styleToolButton(add_stack_btn_, UiIcons::listAdd(this),
                    trInk("Dodaj rząd kopii"));
    styleToolButton(remove_stack_btn_, UiIcons::listRemove(this),
                    trInk("Usuń ostatni rząd kopii"));
    connect(add_stack_btn_, &QPushButton::clicked, this, &MainWindow::onAddCopyStack);
    connect(remove_stack_btn_, &QPushButton::clicked, this, &MainWindow::onRemoveCopyStack);
    copies_row->addWidget(add_stack_btn_);
    copies_row->addWidget(remove_stack_btn_);
    copies_v->addLayout(copies_row);
    auto_copies_chk_ = new QCheckBox(trInk("Wypełnij rząd (auto copies)"), copies_gb);
    auto_scale_chk_ = new QCheckBox(trInk("Dopasuj do obszaru (auto scale)"), copies_gb);
    copies_v->addWidget(auto_copies_chk_);
    copies_v->addWidget(auto_scale_chk_);
    gv->addWidget(copies_gb);

    auto* rot_gb = new QGroupBox(trInk("Obrót grafiki"), graphic_tab);
    auto* rot_form = new QFormLayout(rot_gb);
    rotation_spin_ = new QDoubleSpinBox(rot_gb);
    rotation_spin_->setRange(-180, 180);
    rotation_spin_->setWrapping(true);
    rotation_spin_->setSingleStep(15);
    rotation_spin_->setSuffix(QStringLiteral(" °"));
    auto_rotate_chk_ = new QCheckBox(trInk("Obróć aby zaoszczędzić miejsce"), rot_gb);
    rot_form->addRow(rotation_spin_);
    rot_form->addRow(auto_rotate_chk_);
    gv->addWidget(rot_gb);

    auto* mir_gb = new QGroupBox(trInk("Lustrzane odbicie"), graphic_tab);
    auto* mir_v = new QVBoxLayout(mir_gb);
    mirror_x_chk_ = new QCheckBox(trInk("Lustro względem osi X"), mir_gb);
    mirror_y_chk_ = new QCheckBox(trInk("Lustro względem osi Y"), mir_gb);
    mir_v->addWidget(mirror_x_chk_);
    mir_v->addWidget(mirror_y_chk_);
    gv->addWidget(mir_gb);

    auto* pos_gb = new QGroupBox(trInk("Pozycja na materiale"), graphic_tab);
    auto* pos_form = new QFormLayout(pos_gb);
    layout_offset_x_spin_ = new QDoubleSpinBox(pos_gb);
    layout_offset_y_spin_ = new QDoubleSpinBox(pos_gb);
    for (QDoubleSpinBox* s : {layout_offset_x_spin_, layout_offset_y_spin_}) {
        s->setRange(-99999, 99999);
        s->setDecimals(2);
        s->setSuffix(QStringLiteral(" mm"));
    }
    pos_form->addRow(trInk("Przesunięcie X"), layout_offset_x_spin_);
    pos_form->addRow(trInk("Przesunięcie Y"), layout_offset_y_spin_);
    auto* drag_hint = new QLabel(trInk("Lub przeciągnij grafikę w podglądzie."), pos_gb);
    drag_hint->setWordWrap(true);
    pos_form->addRow(drag_hint);
    gv->addWidget(pos_gb);
    connect(layout_offset_x_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onLayoutChanged);
    connect(layout_offset_y_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onLayoutChanged);

    gv->addStretch(1);
    left_tabs_->addTab(graphic_tab, trInk("Grafika"));

    auto* layers_tab = new QWidget(left_tabs_);
    auto* layers_scroll = new QScrollArea(layers_tab);
    layers_scroll->setWidgetResizable(true);
    layers_scroll->setFrameShape(QFrame::NoFrame);
    auto* layers_inner = new QWidget(layers_scroll);
    layers_scroll->setWidget(layers_inner);
    auto* llv = new QVBoxLayout(layers_inner);
    llv->setSpacing(8);
    auto* layers_hint = new QLabel(
        trInk("Zaznacz warstwę/kolor i ustaw × (przyciski −/+). "
              "Bez warstw Inkscape: „Cały dokument”."),
        layers_inner);
    layers_hint->setWordWrap(true);
    llv->addWidget(layers_hint);
    layer_list_ = new QListWidget(layers_inner);
    layer_list_->setObjectName(QStringLiteral("filter_pass_list"));
    layer_list_->setSelectionMode(QAbstractItemView::NoSelection);
    layer_list_->setMinimumHeight(120);
    llv->addWidget(layer_list_, 2);
    llv->addWidget(new QLabel(trInk("Kolory wypełnienia"), layers_inner));
    fill_color_list_ = new QListWidget(layers_inner);
    fill_color_list_->setObjectName(QStringLiteral("filter_pass_list"));
    fill_color_list_->setSelectionMode(QAbstractItemView::NoSelection);
    llv->addWidget(fill_color_list_, 1);
    llv->addWidget(new QLabel(trInk("Kolory obrysu"), layers_inner));
    stroke_color_list_ = new QListWidget(layers_inner);
    stroke_color_list_->setObjectName(QStringLiteral("filter_pass_list"));
    stroke_color_list_->setSelectionMode(QAbstractItemView::NoSelection);
    llv->addWidget(stroke_color_list_, 1);
    auto* layers_root = new QVBoxLayout(layers_tab);
    layers_root->setContentsMargins(0, 0, 0, 0);
    layers_root->addWidget(layers_scroll);
    dxf_layers_hint_ = new QLabel(QString(), layers_inner);
    dxf_layers_hint_->setWordWrap(true);
    dxf_layers_hint_->hide();
    llv->insertWidget(1, dxf_layers_hint_);
    left_tabs_->addTab(layers_tab, trInk("Warstwy"));
    connect(layer_list_, &QListWidget::itemChanged, this, &MainWindow::onLayerOrColorFilterChanged);
    connect(fill_color_list_, &QListWidget::itemChanged, this,
            &MainWindow::onLayerOrColorFilterChanged);
    connect(stroke_color_list_, &QListWidget::itemChanged, this,
            &MainWindow::onLayerOrColorFilterChanged);

    connect(scale_pct_x_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double pct) {
                scale_x_spin_->setValue(pct / 100.0);
                if (lock_scale_chk_->isChecked()) {
                    QSignalBlocker b(scale_pct_y_spin_);
                    scale_pct_y_spin_->setValue(pct);
                    scale_y_spin_->setValue(pct / 100.0);
                }
                onLayoutChanged();
            });
    connect(scale_pct_y_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double pct) {
                scale_y_spin_->setValue(pct / 100.0);
                onLayoutChanged();
            });
    connect(rotation_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onLayoutChanged);
    connect(copies_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onLayoutChanged);
    connect(auto_copies_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(auto_scale_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(lock_scale_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(mirror_x_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(mirror_y_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(auto_rotate_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);

    auto* weedlines_tab = new QWidget(left_tabs_);
    auto* wlv = new QVBoxLayout(weedlines_tab);
    wlv->setSpacing(6);

    auto make_mm_spin = [weedlines_tab]() {
        auto* s = new QDoubleSpinBox(weedlines_tab);
        s->setRange(0, 500);
        s->setDecimals(2);
        s->setSingleStep(0.1);
        s->setSuffix(QStringLiteral(" mm"));
        return s;
    };

    wlv->addWidget(new QLabel(trInk("Odstęp grafiki"), weedlines_tab));
    auto* spacing_form = new QFormLayout();
    spacing_form->setContentsMargins(0, 0, 0, 0);
    copy_gap_x_spin_ = make_mm_spin();
    copy_gap_y_spin_ = make_mm_spin();
    copy_gap_x_spin_->setValue(10);
    copy_gap_y_spin_->setValue(10);
    spacing_form->addRow(trInk("Rząd"), copy_gap_x_spin_);
    spacing_form->addRow(trInk("Kolumna"), copy_gap_y_spin_);
    wlv->addLayout(spacing_form);

    wlv->addWidget(new QLabel(trInk("Kolejność cięcia"), weedlines_tab));
    order_combo_ = new QComboBox(weedlines_tab);
    order_combo_->addItem(QStringLiteral("Normal"), static_cast<int>(OrderStrategy::Normal));
    order_combo_->addItem(trInk("Odwrócona"), static_cast<int>(OrderStrategy::Reversed));
    order_combo_->addItem(QStringLiteral("Min X"), static_cast<int>(OrderStrategy::MinX));
    order_combo_->addItem(QStringLiteral("Max X"), static_cast<int>(OrderStrategy::MaxX));
    order_combo_->addItem(QStringLiteral("Min Y"), static_cast<int>(OrderStrategy::MinY));
    order_combo_->addItem(QStringLiteral("Max Y"), static_cast<int>(OrderStrategy::MaxY));
    order_combo_->addItem(trInk("Najkrótsza ścieżka"),
                          static_cast<int>(OrderStrategy::ShortestPath));
    order_combo_->addItem(QStringLiteral("Hilbert"), static_cast<int>(OrderStrategy::Hilbert));
    order_combo_->addItem(QStringLiteral("Z-curve"), static_cast<int>(OrderStrategy::ZCurve));
    connect(order_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onLayoutChanged);
    wlv->addWidget(order_combo_);

    wlv->addWidget(new QLabel(trInk("Linie tnące"), weedlines_tab));
    plot_weedline_chk_ =
        new QCheckBox(trInk("Dodaj weedline do całego plotu"), weedlines_tab);
    copy_weedline_chk_ =
        new QCheckBox(trInk("Dodaj weedline wokół kopii"), weedlines_tab);
    wlv->addWidget(plot_weedline_chk_);
    wlv->addWidget(copy_weedline_chk_);

    wlv->addWidget(new QLabel(trInk("Margines weedline (plot)"), weedlines_tab));
    auto* plot_pad_grid = new QGridLayout();
    plot_pad_grid->setContentsMargins(0, 0, 0, 0);
    plot_pad_l_spin_ = make_mm_spin();
    plot_pad_t_spin_ = make_mm_spin();
    plot_pad_r_spin_ = make_mm_spin();
    plot_pad_b_spin_ = make_mm_spin();
    for (QDoubleSpinBox* s : {plot_pad_l_spin_, plot_pad_t_spin_, plot_pad_r_spin_, plot_pad_b_spin_})
        s->setValue(10);
    plot_pad_grid->addWidget(new QLabel(trInk("Lewy"), weedlines_tab), 0, 0);
    plot_pad_grid->addWidget(plot_pad_l_spin_, 0, 1);
    plot_pad_grid->addWidget(new QLabel(trInk("Górny"), weedlines_tab), 0, 2);
    plot_pad_grid->addWidget(plot_pad_t_spin_, 0, 3);
    plot_pad_grid->addWidget(new QLabel(trInk("Prawy"), weedlines_tab), 1, 0);
    plot_pad_grid->addWidget(plot_pad_r_spin_, 1, 1);
    plot_pad_grid->addWidget(new QLabel(trInk("Dolny"), weedlines_tab), 1, 2);
    plot_pad_grid->addWidget(plot_pad_b_spin_, 1, 3);
    wlv->addLayout(plot_pad_grid);

    wlv->addWidget(new QLabel(trInk("Margines weedline (kopia)"), weedlines_tab));
    auto* pad_grid = new QGridLayout();
    pad_grid->setContentsMargins(0, 0, 0, 0);
    copy_pad_l_spin_ = make_mm_spin();
    copy_pad_t_spin_ = make_mm_spin();
    copy_pad_r_spin_ = make_mm_spin();
    copy_pad_b_spin_ = make_mm_spin();
    for (QDoubleSpinBox* s :
         {copy_pad_l_spin_, copy_pad_t_spin_, copy_pad_r_spin_, copy_pad_b_spin_})
        s->setValue(10);
    pad_grid->addWidget(new QLabel(trInk("Lewy"), weedlines_tab), 0, 0);
    pad_grid->addWidget(copy_pad_l_spin_, 0, 1);
    pad_grid->addWidget(new QLabel(trInk("Górny"), weedlines_tab), 0, 2);
    pad_grid->addWidget(copy_pad_t_spin_, 0, 3);
    pad_grid->addWidget(new QLabel(trInk("Prawy"), weedlines_tab), 1, 0);
    pad_grid->addWidget(copy_pad_r_spin_, 1, 1);
    pad_grid->addWidget(new QLabel(trInk("Dolny"), weedlines_tab), 1, 2);
    pad_grid->addWidget(copy_pad_b_spin_, 1, 3);
    wlv->addLayout(pad_grid);

    wlv->addStretch(1);
    left_tabs_->addTab(weedlines_tab, trInk("Linie tnące"));
    connect(copy_gap_x_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onLayoutChanged);
    connect(copy_gap_y_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &MainWindow::onLayoutChanged);
    connect(plot_weedline_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    connect(copy_weedline_chk_, &QCheckBox::toggled, this, &MainWindow::onLayoutChanged);
    for (QDoubleSpinBox* s :
         {copy_pad_l_spin_, copy_pad_t_spin_, copy_pad_r_spin_, copy_pad_b_spin_,
          plot_pad_l_spin_, plot_pad_t_spin_, plot_pad_r_spin_, plot_pad_b_spin_}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onLayoutChanged);
    }

    auto* left_dock = new QDockWidget(this);
    left_dock->setObjectName(QStringLiteral("dock_left"));
    left_dock->setWidget(left_tabs_);
    left_dock->setTitleBarWidget(new QWidget(left_dock));
    left_dock->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, left_dock);

    device_host_ = new QWidget(this);
    device_host_->hide();
    preset_combo_ = new QComboBox(device_host_);
    for (const DevicePreset& p : devicePresets())
        preset_combo_->addItem(QStringLiteral("%1 %2").arg(p.manufacturer, p.model), p.id);
    connect(preset_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPresetChanged);
    transport_combo_ = new QComboBox(device_host_);
    transport_combo_->addItem(trInk("Port szeregowy"),
                              int(PlotTransportKind::SerialPort));
    transport_combo_->addItem(trInk("Zapis do pliku"), int(PlotTransportKind::FileOutput));
    connect(transport_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onTransportChanged);
    port_edit_ = new QLineEdit(QStringLiteral("/dev/ttyUSB0"), device_host_);
    baud_spin_ = new QSpinBox(device_host_);
    baud_spin_->setRange(1200, 1000000);
    baud_spin_->setValue(115200);
    output_path_edit_ = new QLineEdit(device_host_);
    printer_edit_ = new QLineEdit(device_host_);
    plugin_combo_ = new QComboBox(device_host_);
    plugin_refresh_btn_ = new QPushButton(device_host_);
    styleToolButton(plugin_refresh_btn_, UiIcons::refresh(this),
                    trInk("Odśwież listę wtyczek urządzeń"), false);
    plugin_refresh_btn_->setText(trInk("Odśwież"));
    connect(plugin_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPluginComboChanged);

    bottom_tabs_ = new QTabWidget(this);
    bottom_tabs_->setTabPosition(QTabWidget::South);
    bottom_tabs_->setDocumentMode(true);

    auto* jobs_tab = new QWidget(bottom_tabs_);
    auto* hv = new QVBoxLayout(jobs_tab);
    history_table_ = new QTableWidget(jobs_tab);
    history_table_->setColumnCount(9);
    history_table_->setHorizontalHeaderLabels(
        {trInk("Data"), trInk("Dokument"), trInk("Liczba"),
         trInk("Czas"), trInk("Status"), trInk("Kopie"),
         trInk("Obrót"), trInk("Rozmiar"), trInk("Materiał")});
    history_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    history_table_->horizontalHeader()->setStretchLastSection(true);
    history_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_table_->setAlternatingRowColors(true);
    history_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(history_table_, &QTableWidget::customContextMenuRequested, this,
            &MainWindow::showJobHistoryContextMenu);
    connect(history_table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { openJobHistoryRow(row); });
    hv->addWidget(history_table_, 1);
    bottom_tabs_->addTab(jobs_tab, trInk("Zadania"));

    auto* live_tab = new QWidget(bottom_tabs_);
    auto* liv = new QVBoxLayout(live_tab);
    liv->setContentsMargins(0, 0, 0, 0);
    liv->setSpacing(0);

    live_scene_ = new QGraphicsScene(live_tab);
    live_view_ = new LivePlotView(live_tab);
    live_view_->setScene(live_scene_);
    live_view_->setAxisUnitMm(1.0);
    live_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(live_view_, &QWidget::customContextMenuRequested, this,
            &MainWindow::showLivePlotContextMenu);
    liv->addWidget(attachPlotZoomBar(live_tab, live_view_));
    liv->addWidget(live_view_, 1);

    auto* status_container = new QWidget(live_tab);
    auto* status_outer = new QVBoxLayout(status_container);
    status_outer->setContentsMargins(4, 4, 4, 4);
    status_outer->setSpacing(4);

    auto* status_row = new QHBoxLayout();
    status_row->setSpacing(8);
    live_source_label_ = new QLabel(QStringLiteral("Source: None"), status_container);
    live_size_label_ = new QLabel(status_container);
    live_duration_label_ = new QLabel(status_container);
    live_left_label_ = new QLabel(status_container);
    live_eta_label_ = new QLabel(status_container);
    live_source_label_->setWordWrap(true);
    status_row->addWidget(live_source_label_, 2);
    status_row->addWidget(live_size_label_, 1);
    status_row->addWidget(live_duration_label_, 1);
    status_row->addWidget(live_left_label_, 1);
    status_row->addWidget(live_eta_label_, 1);
    status_outer->addLayout(status_row);
    plot_position_label_ = new QLabel(status_container);
    plot_position_label_->setWordWrap(true);
    status_outer->addWidget(plot_position_label_);

    auto* prog_row = new QHBoxLayout();
    live_progress_bar_ = new QProgressBar(status_container);
    live_progress_bar_->setRange(0, 100);
    live_progress_bar_->setValue(0);
    live_progress_bar_->setTextVisible(true);
    live_abort_btn_ = new QPushButton(status_container);
    live_abort_btn_->setVisible(false);
    connect(live_abort_btn_, &QPushButton::clicked, this, &MainWindow::onSendCancel);
    live_action_btn_ = new QPushButton(status_container);
    connect(live_action_btn_, &QPushButton::clicked, this, &MainWindow::onLiveActionClicked);
    applyLiveActionUi(UiIcons::LiveAction::Start);
    styleToolButton(live_abort_btn_, UiIcons::liveAction(UiIcons::LiveAction::Stop, this),
                    trInk("Przerwij wysyłkę"), false);
    live_abort_btn_->setText(trInk("Przerwij"));
    prog_row->addWidget(live_progress_bar_, 1);
    prog_row->addWidget(live_abort_btn_);
    prog_row->addWidget(live_action_btn_);
    status_outer->addLayout(prog_row);
    liv->addWidget(status_container);

    send_pause_btn_ = live_action_btn_;
    send_cancel_btn_ = live_abort_btn_;

    live_status_timer_ = new QTimer(this);
    live_status_timer_->setInterval(500);
    connect(live_status_timer_, &QTimer::timeout, this, &MainWindow::updateLiveStatusTick);

    bottom_tabs_->addTab(live_tab, trInk("Live"));

    auto* monitor_tab = new QWidget(bottom_tabs_);
    auto* mv = new QVBoxLayout(monitor_tab);
    auto* mon_top = new QHBoxLayout();
    monitor_log_send_chk_ = new QCheckBox(trInk("Loguj TX/RX"), monitor_tab);
    monitor_log_send_chk_->setChecked(true);
    monitor_hex_chk_ = new QCheckBox(trInk("Hex"), monitor_tab);
    auto* mon_clear = new QPushButton(monitor_tab);
    styleToolButton(mon_clear, UiIcons::clear(this), trInk("Wyczyść monitor"), false);
    mon_clear->setText(trInk("Wyczyść"));
    connect(mon_clear, &QPushButton::clicked, this, &MainWindow::onMonitorClear);
    mon_top->addWidget(monitor_log_send_chk_);
    mon_top->addWidget(monitor_hex_chk_);
    mon_top->addStretch(1);
    mon_top->addWidget(mon_clear);
    mv->addLayout(mon_top);
    monitor_edit_ = new QPlainTextEdit(monitor_tab);
    monitor_edit_->setReadOnly(true);
    monitor_edit_->setMaximumBlockCount(8000);
    QFont mono = monitor_edit_->font();
    mono.setFamily(QStringLiteral("monospace"));
    monitor_edit_->setFont(mono);
    monitor_live_chk_ = new QCheckBox(trInk("Nasłuch portu (tylko odczyt)"), monitor_tab);
    connect(monitor_live_chk_, &QCheckBox::toggled, this, &MainWindow::onLiveListenToggled);
    mv->addWidget(monitor_live_chk_);
    mv->addWidget(monitor_edit_, 1);
    bottom_tabs_->addTab(monitor_tab, trInk("Monitor"));

    auto* console_tab = new QWidget(bottom_tabs_);
    auto* cv = new QVBoxLayout(console_tab);
    console_output_ = new QPlainTextEdit(console_tab);
    console_output_->setReadOnly(true);
    console_output_->setMaximumBlockCount(500);
    console_output_->appendPlainText(
        trInk("Konsola urządzenia — wpisz komendę (HPGL/G-code) i Enter."));
    console_input_ = new QLineEdit(console_tab);
    console_input_->setPlaceholderText(trInk("np. PG; lub G0 X10 Y10"));
    connect(console_input_, &QLineEdit::returnPressed, this, &MainWindow::onConsoleCommand);
    cv->addWidget(console_output_, 1);
    cv->addWidget(console_input_);
    bottom_tabs_->addTab(console_tab, trInk("Konsola"));

    control_tab_ = new QWidget(bottom_tabs_);
    control_tab_->setObjectName(QStringLiteral("control_tab"));
    auto* ctrlv = new QVBoxLayout(control_tab_);
    ctrlv->setSpacing(6);

    auto* main_row = new QHBoxLayout();
    auto* directions = new QWidget(control_tab_);
    auto* dir_v = new QVBoxLayout(directions);
    dir_v->addWidget(new QLabel(trInk("Sterowanie")));
    control_grid_ = new QGridLayout();
    control_grid_->setSpacing(metricsFor(app_settings_.ui_profile).control_grid_spacing);

    auto make_ctl_btn = [this]() {
        auto* b = new QPushButton(control_tab_);
        b->setMinimumSize(metricsFor(app_settings_.ui_profile).control_button_min_px,
                          metricsFor(app_settings_.ui_profile).control_button_min_px);
        return b;
    };

    auto* btn_pen_up = make_ctl_btn();
    styleToolButton(btn_pen_up, UiIcons::penUp(this),
                    trInk("Podnieś głowicę (pen up)"));
    auto* btn_set_origin = make_ctl_btn();
    styleToolButton(btn_set_origin, UiIcons::setOrigin(this),
                    trInk("Ustaw wirtualny początek"));
    auto* btn_up = make_ctl_btn();
    styleToolButton(btn_up, UiIcons::moveUp(this), trInk("Przesuń w górę"));
    auto* btn_go_origin = make_ctl_btn();
    styleToolButton(btn_go_origin, UiIcons::goOrigin(this),
                    trInk("Wróć do wirtualnego początku"));
    auto* btn_sys_origin = make_ctl_btn();
    styleToolButton(btn_sys_origin, UiIcons::systemOrigin(this),
                    trInk("Wróć do początku systemu (0,0)"));

    auto* btn_pen_down = make_ctl_btn();
    styleToolButton(btn_pen_down, UiIcons::penDown(this),
                    trInk("Opuść głowicę (pen down)"));
    auto* btn_left = make_ctl_btn();
    styleToolButton(btn_left, UiIcons::moveLeft(this), trInk("Przesuń w lewo"));
    auto* btn_down = make_ctl_btn();
    styleToolButton(btn_down, UiIcons::moveDown(this), trInk("Przesuń w dół"));
    auto* btn_right = make_ctl_btn();
    styleToolButton(btn_right, UiIcons::moveRight(this), trInk("Przesuń w prawo"));
    control_connect_btn_ = make_ctl_btn();
    control_connect_btn_->setCheckable(true);
    applyControlConnectUi(false);

    control_grid_->addWidget(btn_pen_up, 0, 0);
    control_grid_->addWidget(btn_set_origin, 0, 1);
    control_grid_->addWidget(btn_up, 0, 2);
    control_grid_->addWidget(btn_go_origin, 0, 3);
    control_grid_->addWidget(btn_sys_origin, 0, 4);
    control_grid_->addWidget(btn_pen_down, 1, 0);
    control_grid_->addWidget(btn_left, 1, 1);
    control_grid_->addWidget(btn_down, 1, 2);
    control_grid_->addWidget(btn_right, 1, 3);
    control_grid_->addWidget(control_connect_btn_, 1, 4);

    connect(btn_pen_up, &QPushButton::clicked, this, &MainWindow::onControlPenUp);
    connect(btn_pen_down, &QPushButton::clicked, this, &MainWindow::onControlPenDown);
    connect(btn_set_origin, &QPushButton::clicked, this, &MainWindow::onControlSetOrigin);
    connect(btn_go_origin, &QPushButton::clicked, this, &MainWindow::onControlGoOrigin);
    connect(btn_sys_origin, &QPushButton::clicked, this, &MainWindow::onControlGoSystemOrigin);
    connect(btn_up, &QPushButton::clicked, this, &MainWindow::onControlMoveUp);
    connect(btn_down, &QPushButton::clicked, this, &MainWindow::onControlMoveDown);
    connect(btn_left, &QPushButton::clicked, this, &MainWindow::onControlMoveLeft);
    connect(btn_right, &QPushButton::clicked, this, &MainWindow::onControlMoveRight);
    connect(control_connect_btn_, &QPushButton::toggled, this, &MainWindow::onControlConnectToggle);

    dir_v->addLayout(control_grid_);

    auto* material_row = new QHBoxLayout();
    material_row->setSpacing(metricsFor(app_settings_.ui_profile).control_grid_spacing);
    control_load_btn_ = new QPushButton(trInk("Załaduj"), control_tab_);
    control_unload_btn_ = new QPushButton(trInk("Wyładuj"), control_tab_);
    control_load_btn_->setMinimumHeight(
        metricsFor(app_settings_.ui_profile).control_button_min_px);
    control_unload_btn_->setMinimumHeight(
        metricsFor(app_settings_.ui_profile).control_button_min_px);
    styleToolButton(control_load_btn_, UiIcons::materialLoad(this),
                    trInk("Załaduj materiał na ploter (komenda z ustawień)"), false);
    styleToolButton(control_unload_btn_, UiIcons::materialUnload(this),
                    trInk("Wyładuj materiał z plotera (komenda z ustawień)"), false);
    connect(control_load_btn_, &QPushButton::clicked, this, &MainWindow::onControlMaterialLoad);
    connect(control_unload_btn_, &QPushButton::clicked, this, &MainWindow::onControlMaterialUnload);
    material_row->addWidget(control_load_btn_, 1);
    material_row->addWidget(control_unload_btn_, 1);
    dir_v->addLayout(material_row);

    main_row->addWidget(directions);

    auto* status_box = new QGroupBox(trInk("Status"), control_tab_);
    auto* status_form = new QFormLayout(status_box);
    auto* device_lbl = new QLabel(port_edit_->text(), status_box);
    device_lbl->setWordWrap(true);
    control_step_spin_ = new QSpinBox(status_box);
    control_step_spin_->setRange(1, 1000000000);
    control_step_spin_->setSingleStep(10);
    control_step_spin_->setValue(100);
    control_status_label_ = new QLabel(status_box);
    control_status_label_->setWordWrap(true);
    status_form->addRow(trInk("Urządzenie"), device_lbl);
    status_form->addRow(QStringLiteral("Krok"), control_step_spin_);
    status_form->addRow(QStringLiteral("Stan"), control_status_label_);
    main_row->addWidget(status_box, 1);
    ctrlv->addLayout(main_row);
    ctrlv->addStretch(1);

    control_pos_ = QPointF(0, 0);
    control_origin_ = QPointF(0, 0);
    updateControlStatusLabel();
    bottom_tabs_->addTab(control_tab_, trInk("Sterowanie"));

    bottom_dock_ = new QDockWidget(this);
    bottom_dock_->setObjectName(QStringLiteral("dock_bottom"));
    bottom_dock_->setWidget(bottom_tabs_);
    bottom_dock_->setTitleBarWidget(new QWidget(bottom_dock_));
    addDockWidget(Qt::BottomDockWidgetArea, bottom_dock_);

    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    pipeline_cache_.velocity = 120;

    plugin_loader_ = std::make_unique<DevicePluginLoader>();
    refreshDevicePlugins();
    loadAppSettings(app_settings_);
    loadPersistedSettings();
    applyAppSettingsToUi();
    applyUiProfile();
    onTransportChanged(0);
    rebuildJobHistoryUi();
    updateFilePathLabel();
    rebuildPreview();
}

MainWindow::~MainWindow()
{
    stopSendWorker();
    stopControlConnection();
    stopLiveListen();
}

PlotJobSettings MainWindow::collectJobSettings() const
{
    PlotJobSettings s = pipeline_cache_;
    s.flatten_step = app_settings_.flatten_step;

    if (order_combo_)
        s.order = static_cast<OrderStrategy>(order_combo_->currentData().toInt());

    const QString& u = app_settings_.units;
    s.material.width = displayToMm(mat_w_spin_->value(), u);
    s.material.height = displayToMm(mat_h_spin_->value(), u);
    s.material.padding_left = displayToMm(mat_pad_l_spin_->value(), u);
    s.material.padding_top = displayToMm(mat_pad_t_spin_->value(), u);
    s.material.padding_right = displayToMm(mat_pad_r_spin_->value(), u);
    s.material.padding_bottom = displayToMm(mat_pad_b_spin_->value(), u);
    s.material.is_roll = mat_roll_chk_ && mat_roll_chk_->isChecked();
    s.material.use_custom_force_speed =
        mat_force_speed_chk_ && mat_force_speed_chk_->isChecked();
    if (mat_force_spin_)
        s.material.force = mat_force_spin_->value();
    if (mat_speed_spin_)
        s.material.speed = mat_speed_spin_->value();

    s.layout.scale_x = scale_pct_x_spin_->value() / 100.0;
    s.layout.scale_y = scale_pct_y_spin_->value() / 100.0;
    s.layout.rotation_deg = rotation_spin_->value();
    s.layout.copies = copies_spin_->value();
    s.layout.copy_spacing_x = displayToMm(copy_gap_x_spin_->value(), u);
    s.layout.copy_spacing_y = displayToMm(copy_gap_y_spin_->value(), u);
    s.layout.mirror_x = mirror_x_chk_->isChecked();
    s.layout.mirror_y = mirror_y_chk_->isChecked();
    s.layout.auto_rotate = auto_rotate_chk_->isChecked();
    s.layout.align_center_x = align_center_x_chk_->isChecked();
    s.layout.align_center_y = align_center_y_chk_->isChecked();
    s.layout.auto_shift = auto_shift_chk_->isChecked();
    s.layout.auto_copies = auto_copies_chk_->isChecked();
    s.layout.auto_scale = auto_scale_chk_ && auto_scale_chk_->isChecked();
    s.layout.lock_scale = lock_scale_chk_->isChecked();
    if (layout_offset_x_spin_)
        s.layout.layout_offset_x = layout_offset_x_spin_->value();
    if (layout_offset_y_spin_)
        s.layout.layout_offset_y = layout_offset_y_spin_->value();

    s.feed_to_end = feed_after_rb_ && feed_after_rb_->isChecked();
    s.feed_after = feed_after_spin_ ? displayToMm(feed_after_spin_->value(), u) : 0;

    s.weedlines.plot_weedline = plot_weedline_chk_->isChecked();
    s.weedlines.copy_weedline = copy_weedline_chk_->isChecked();
    if (plot_pad_l_spin_) {
        s.weedlines.plot_pad_left = displayToMm(plot_pad_l_spin_->value(), u);
        s.weedlines.plot_pad_top = displayToMm(plot_pad_t_spin_->value(), u);
        s.weedlines.plot_pad_right = displayToMm(plot_pad_r_spin_->value(), u);
        s.weedlines.plot_pad_bottom = displayToMm(plot_pad_b_spin_->value(), u);
    }
    s.weedlines.copy_pad_left = displayToMm(copy_pad_l_spin_->value(), u);
    s.weedlines.copy_pad_top = displayToMm(copy_pad_t_spin_->value(), u);
    s.weedlines.copy_pad_right = displayToMm(copy_pad_r_spin_->value(), u);
    s.weedlines.copy_pad_bottom = displayToMm(copy_pad_b_spin_->value(), u);

    if (plugin_combo_ && plugin_combo_->currentIndex() > 0)
        s.plugin_id = plugin_combo_->currentData().toString();
    else
        s.plugin_id.clear();

    s.device = active_device_;
    s.device.transport =
        static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
    s.device.port_name = port_edit_->text();
    s.device.baud_rate = baud_spin_->value();
    s.device.output_path = output_path_edit_->text();
    s.device.printer_name = printer_edit_->text().trimmed();
    s.device.preset_id = preset_combo_->currentData().toString();

    if (layer_list_) {
        s.layer_filters.clear();
        for (int i = 0; i < layer_list_->count(); ++i) {
            QListWidgetItem* it = layer_list_->item(i);
            LayerFilterEntry e;
            e.layer_id = it->data(Qt::UserRole).toString();
            e.pass_count = 1;
            if (QWidget* row = layer_list_->itemWidget(it)) {
                if (auto* chk = row->findChild<QCheckBox*>()) {
                    e.enabled = chk->isChecked();
                    e.name = chk->text();
                }
                if (auto* spin = row->findChild<QSpinBox*>())
                    e.pass_count = spin->value();
            } else {
                e.name = it->text();
                e.enabled = it->checkState() == Qt::Checked;
            }
            s.layer_filters.push_back(e);
        }
    }
    s.color_filters.clear();
    auto collect_colors = [&](QListWidget* list, bool is_fill) {
        if (!list)
            return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* it = list->item(i);
            ColorFilterEntry e;
            e.color_key = it->data(Qt::UserRole).toString();
            e.is_fill = is_fill;
            e.pass_count = 1;
            if (QWidget* row = list->itemWidget(it)) {
                if (auto* chk = row->findChild<QCheckBox*>())
                    e.enabled = chk->isChecked();
                if (auto* spin = row->findChild<QSpinBox*>())
                    e.pass_count = spin->value();
            } else {
                e.enabled = it->checkState() == Qt::Checked;
            }
            s.color_filters.push_back(e);
        }
    };
    collect_colors(fill_color_list_, true);
    collect_colors(stroke_color_list_, false);
    return s;
}

void MainWindow::pushRecentPath(const QString& path)
{
    if (path.isEmpty())
        return;
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    QStringList list = settings.value(QStringLiteral("recent_files")).toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 12)
        list.removeLast();
    settings.setValue(QStringLiteral("recent_files"), list);
}

void MainWindow::rebuildRecentMenu()
{
    if (!recent_menu_)
        return;
    recent_menu_->clear();
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    const QStringList list = settings.value(QStringLiteral("recent_files")).toStringList();
    if (list.isEmpty()) {
        QAction* placeholder = recent_menu_->addAction(QStringLiteral("(brak)"));
        placeholder->setEnabled(false);
        return;
    }
    for (const QString& path : list) {
        QAction* a = recent_menu_->addAction(path);
        a->setData(path);
        connect(a, &QAction::triggered, this, &MainWindow::onRecentFileTriggered);
    }
}

bool MainWindow::loadCurrentDesign(QPainterPath& out, QString& err, QStringList* warns)
{
    if (current_file_.isEmpty()) {
        err = QStringLiteral("Brak pliku.");
        return false;
    }
    const PlotJobSettings job = collectJobSettings();
    if (!current_svg_xml_.isEmpty()) {
        return loadSvgDesignPath(current_svg_xml_, QFileInfo(current_file_).absolutePath(), job,
                                 out, &err, warns);
    }
    if (current_file_.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive)) {
        QFile f(current_file_);
        if (!f.open(QIODevice::ReadOnly)) {
            err = trInk("Nie można otworzyć pliku.");
            return false;
        }
        if (!loadDxfPainterPathFromBytes(f.readAll(), out, &err))
            return false;
        const int passes = qMax(maxEnabledLayerPassCount(job.layer_filters),
                                maxEnabledColorPassCount(job.color_filters));
        if (passes > 1) {
            RepeatFilterConfig rc = job.repeat;
            rc.steps = passes;
            out = applyRepeatFilter(out, rc);
        }
        return true;
    }
    QString xml = current_svg_xml_;
    if (xml.isEmpty())
        xml = readFileUtf8(current_file_);
    return loadSvgDesignPath(xml, QFileInfo(current_file_).absolutePath(), job, out, &err, warns);
}

bool MainWindow::applyOpenedDesign(const QString& path)
{
    QString err;
    QPainterPath path_model;
    QStringList warns;
    current_file_ = path;
    current_svg_xml_.clear();
    design_is_filled_trace_ = false;
    if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
        current_svg_xml_ = readFileUtf8(path);
    } else if (path.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive)
               && isDxfToSvgAvailable()) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, QStringLiteral("Inkcut"),
                                 trInk("Nie można otworzyć pliku DXF."));
            return false;
        }
        QString svg_xml;
        QString conv_err;
        if (!convertDxfBytesToSvg(f.readAll(), svg_xml, &conv_err)) {
            QMessageBox::warning(this, QStringLiteral("Inkcut"), conv_err);
            return false;
        }
        current_svg_xml_ = svg_xml;
    }

    if (!loadCurrentDesign(path_model, err, &warns)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);
        return false;
    }

    PlotJobSettings job = collectJobSettings();
    job.layout.layout_offset_x = 0;
    job.layout.layout_offset_y = 0;
    job.layout.scale_x = 1.0;
    job.layout.scale_y = 1.0;
    job.layout.lock_scale = true;
    job.layout.align_center_x = true;
    job.layout.align_center_y = true;
    applyJobSettingsToUi(job);

    pushRecentPath(path);
    rebuildRecentMenu();
    refreshLayerAndColorLists();
    rebuildPreview();
    updateFilePathLabel();
    showSvgWarningsIfAny(warns);
    return true;
}

void MainWindow::updateFilePathLabel()
{
    if (!file_path_label_)
        return;
    if (current_file_.isEmpty())
        file_path_label_->setText(trInk("Brak wczytanego pliku."));
    else
        file_path_label_->setText(QFileInfo(current_file_).absoluteFilePath());
}

void MainWindow::onRecentFileTriggered()
{
    QAction* a = qobject_cast<QAction*>(sender());
    if (!a)
        return;
    applyOpenedDesign(a->data().toString());
}

void MainWindow::appendMonitorRaw(const QByteArray& data, bool is_tx)
{
    if (!monitor_edit_)
        return;
    const QString line = (is_tx ? QStringLiteral("[TX] ") : QStringLiteral("[RX] "))
        + format_monitor_chunk(data, monitor_hex_chk_->isChecked()) + QLatin1Char('\n');
    monitor_edit_->appendPlainText(line);
    if (!is_tx && monitor_hex_chk_ && !monitor_hex_chk_->isChecked())
        ingestRxPlotterHints(data);
}

void MainWindow::drainSerialToMonitor(QSerialPort& serial)
{
    if (!monitor_log_send_chk_->isChecked())
        return;
    const QByteArray r = serial.readAll();
    if (!r.isEmpty())
        appendMonitorRaw(r, false);
}

void MainWindow::onMonitorClear()
{
    if (monitor_edit_)
        monitor_edit_->clear();
    rx_accum_.clear();
}

void MainWindow::stopLiveListen()
{
    if (!live_serial_)
        return;
    live_serial_->disconnect();
    live_serial_->close();
    live_serial_.reset();
}

void MainWindow::applyLiveActionUi(UiIcons::LiveAction action)
{
    if (!live_action_btn_)
        return;

    QString text;
    switch (action) {
    case UiIcons::LiveAction::Start:
        text = QStringLiteral("Start");
        break;
    case UiIcons::LiveAction::Pause:
        text = QStringLiteral("Pauza");
        break;
    case UiIcons::LiveAction::Resume:
        text = trInk("Wznów");
        break;
    case UiIcons::LiveAction::Stop:
        return;
    }

    live_action_btn_->setText(text);
    live_action_btn_->setIcon(UiIcons::liveAction(action, live_action_btn_));
    live_action_btn_->setIconSize(QSize(20, 20));
}

void MainWindow::applyControlConnectUi(bool connected)
{
    if (!control_connect_btn_)
        return;
    styleToolButton(control_connect_btn_, UiIcons::deviceConnect(connected, control_connect_btn_),
                    connected ? trInk("Rozłącz od plotera")
                              : trInk("Połącz z ploterem"));
}

void MainWindow::stopControlConnection()
{
    if (control_serial_) {
        control_serial_->close();
        control_serial_.reset();
    }
    if (control_connect_btn_) {
        QSignalBlocker b(control_connect_btn_);
        control_connect_btn_->setChecked(false);
        applyControlConnectUi(false);
    }
}

void MainWindow::onLiveListenToggled(bool on)
{
    if (!on) {
        stopLiveListen();
        return;
    }

    stopControlConnection();

    live_serial_ = std::make_unique<QSerialPort>();
    SerialOpenOptions opt = serial_open_options_from_device(active_device_);
    opt.port_name = port_edit_->text();
    opt.baud_rate = baud_spin_->value();

    if (!open_serial_read_only(*live_serial_, opt)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             trInk("Nie można otworzyć portu do nasłuchu."));
        QSignalBlocker b2(monitor_live_chk_);
        monitor_live_chk_->setChecked(false);
        live_serial_.reset();
        return;
    }

    connect(live_serial_.get(), &QSerialPort::readyRead, this, [this]() {
        if (!live_serial_)
            return;
        const QByteArray r = live_serial_->readAll();
        if (!r.isEmpty())
            appendMonitorRaw(r, false);
    });
}

void MainWindow::updateControlStatusLabel()
{
    if (!control_status_label_)
        return;
    const QString z = control_z_ > 0.5 ? QStringLiteral("down") : QStringLiteral("up");
    control_status_label_->setText(
        trInk("Pozycja: (x=%1, y=%2, z=%3)  Początek: (x=%4, y=%5)  %6")
            .arg(control_pos_.x(), 0, 'f', 3)
            .arg(control_pos_.y(), 0, 'f', 3)
            .arg(z)
            .arg(control_origin_.x(), 0, 'f', 3)
            .arg(control_origin_.y(), 0, 'f', 3)
            .arg(control_serial_ && control_serial_->isOpen() ? trInk("połączony")
                                                              : trInk("rozłączony")));
}

bool MainWindow::sendControlRawCommand(const QString& command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Inkcut"),
            trInk("Brak komendy — ustaw ją w Ustawienia → Sterowanie → Załaduj/Wyładuj materiał."));
        return false;
    }

    if (!control_serial_ || !control_serial_->isOpen()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 trInk("Najpierw połącz się z ploterem (przycisk połączenia)."));
        return false;
    }

    QString payload = trimmed;
    payload.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    payload.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
    const QByteArray bytes = payload.toUtf8();

    if (monitor_log_send_chk_->isChecked())
        appendMonitorRaw(bytes, true);

    if (!write_all(*control_serial_, bytes)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             trInk("Zapis komendy sterowania nie powiódł się."));
        return false;
    }

    drainSerialToMonitor(*control_serial_);
    return true;
}

bool MainWindow::sendControlMove(double x, double y, double z)
{
    if (!control_serial_ || !control_serial_->isOpen()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 trInk("Najpierw połącz się z ploterem (przycisk połączenia)."));
        return false;
    }

    const bool prev_pen_up = (control_z_ < 0.5);
    const std::string cmd =
        encode_move_absolute_user_xy(x, y, z, collectJobSettings().protocol, prev_pen_up);
    const QByteArray bytes = QByteArray::fromStdString(cmd);

    if (monitor_log_send_chk_->isChecked())
        appendMonitorRaw(bytes, true);

    if (!write_all(*control_serial_, bytes)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             trInk("Zapis komendy sterowania nie powiódł się."));
        return false;
    }

    drainSerialToMonitor(*control_serial_);
    control_pos_ = QPointF(x, y);
    control_z_ = z;
    updateControlStatusLabel();
    return true;
}

void MainWindow::onControlMaterialLoad()
{
    sendControlRawCommand(app_settings_.material_load_command);
}

void MainWindow::onControlMaterialUnload()
{
    sendControlRawCommand(app_settings_.material_unload_command);
}

void MainWindow::onControlConnectToggle()
{
    if (!control_connect_btn_->isChecked()) {
        stopControlConnection();
        applyControlConnectUi(false);
        updateControlStatusLabel();
        return;
    }

    if (monitor_live_chk_->isChecked()) {
        QSignalBlocker b(monitor_live_chk_);
        monitor_live_chk_->setChecked(false);
    }
    stopLiveListen();

    control_serial_ = std::make_unique<QSerialPort>();
    SerialOpenOptions opt = serial_open_options_from_device(active_device_);
    opt.port_name = port_edit_->text();
    opt.baud_rate = baud_spin_->value();

    if (!open_serial_read_write(*control_serial_, opt)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             trInk("Nie można otworzyć portu %1").arg(opt.port_name));
        control_serial_.reset();
        QSignalBlocker b2(control_connect_btn_);
        control_connect_btn_->setChecked(false);
        return;
    }

    applyControlConnectUi(true);
    updateControlStatusLabel();
    appendMonitorRaw(trInk("[control] połączono %1\n").arg(opt.port_name).toUtf8(), false);
}

void MainWindow::onControlPenUp()
{
    sendControlMove(control_pos_.x(), control_pos_.y(), 0.0);
}

void MainWindow::onControlPenDown()
{
    sendControlMove(control_pos_.x(), control_pos_.y(), 1.0);
}

void MainWindow::onControlSetOrigin()
{
    control_origin_ = control_pos_;
  const PlotJobSettings job = collectJobSettings();
    const std::string cmd =
        encode_set_origin_user_xy(control_pos_.x(), control_pos_.y(), job.protocol);
    if (!cmd.empty())
        sendControlRawCommand(QString::fromStdString(cmd));
    else if (control_serial_ && control_serial_->isOpen())
        appendMonitorRaw(
            QByteArrayLiteral("[info] Ustawiono początek lokalnie (HPGL: brak G92)\n"), false);
    updateControlStatusLabel();
}

void MainWindow::onControlGoOrigin()
{
    sendControlMove(control_origin_.x(), control_origin_.y(), control_z_);
}

void MainWindow::onControlGoSystemOrigin()
{
    sendControlMove(0, 0, control_z_);
}

void MainWindow::onControlMoveUp()
{
    const int step = control_step_spin_->value();
    sendControlMove(control_pos_.x(), control_pos_.y() + step, control_z_);
}

void MainWindow::onControlMoveDown()
{
    const int step = control_step_spin_->value();
    sendControlMove(control_pos_.x(), control_pos_.y() - step, control_z_);
}

void MainWindow::onControlMoveLeft()
{
    const int step = control_step_spin_->value();
    sendControlMove(control_pos_.x() - step, control_pos_.y(), control_z_);
}

void MainWindow::onControlMoveRight()
{
    const int step = control_step_spin_->value();
    sendControlMove(control_pos_.x() + step, control_pos_.y(), control_z_);
}

void MainWindow::onPreviewGraphicOffsetChanged(qreal x, qreal y)
{
    pipeline_cache_.layout.layout_offset_x = x;
    pipeline_cache_.layout.layout_offset_y = y;
    if (layout_offset_x_spin_) {
        QSignalBlocker bx(layout_offset_x_spin_);
        layout_offset_x_spin_->setValue(x);
    }
    if (layout_offset_y_spin_) {
        QSignalBlocker by(layout_offset_y_spin_);
        layout_offset_y_spin_->setValue(y);
    }
}

void MainWindow::onPreviewGraphicDragFinished()
{
    savePersistedSettings();
    rebuildLivePlotScene();
}

void MainWindow::rebuildPreview()
{
    scene_->clear();
    preview_graphic_group_ = nullptr;
    preview_view_->setGraphicDragTarget(nullptr);

    const PlotJobSettings job = collectJobSettings();
    const QPainterPath device_path = deviceAreaPath(job.material);
    const QPainterPath mat_path = materialOutlinePath(job.material);
    const QPainterPath avail_path = materialAvailableAreaPath(job.material);

    scene_->addPath(device_path, QPen(QColor(235, 194, 194), 0, Qt::DashLine));
    scene_->addPath(mat_path, QPen(Qt::black, 0, Qt::SolidLine));
    scene_->addPath(avail_path, QPen(Qt::black, 0, Qt::DashLine));

    QRectF bounds = device_path.boundingRect().united(mat_path.boundingRect());

    if (current_file_.isEmpty()) {
        scene_->setSceneRect(bounds.adjusted(-40, -40, 40, 40));
        if (preview_view_)
            preview_view_->fitAll();
        rebuildLivePlotScene();
        return;
    }

    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err)) {
        scene_->setSceneRect(bounds.adjusted(-40, -40, 40, 40));
        if (preview_view_)
            preview_view_->fitAll();
        rebuildLivePlotScene();
        return;
    }

    PlotJobSettings layout_job = job;
    layout_job.layout.layout_offset_x = 0;
    layout_job.layout.layout_offset_y = 0;
    const QPainterPath job_path =
        design_is_filled_trace_ ? processFilledTracePath(path, layout_job, activePlugin())
                                : processJobPath(path, layout_job, activePlugin());
    preview_graphic_group_ = scene_->createItemGroup({});
    if (design_is_filled_trace_) {
        QPainterPath filled = job_path;
        filled.setFillRule(Qt::OddEvenFill);
        auto* fill_item = new QGraphicsPathItem(filled);
        fill_item->setBrush(QColor(50, 50, 50));
        fill_item->setPen(Qt::NoPen);
        preview_graphic_group_->addToGroup(fill_item);
    } else {
        const MoveCutPaths paths = splitMoveCutPaths(job_path);
        auto* move_item = new QGraphicsPathItem(paths.move);
        move_item->setPen(penPlotMove());
        preview_graphic_group_->addToGroup(move_item);
        auto* cut_item = new QGraphicsPathItem(paths.cut);
        cut_item->setPen(penPlotCut());
        preview_graphic_group_->addToGroup(cut_item);
    }
    scene_->addItem(preview_graphic_group_);
    preview_graphic_group_->setPos(job.layout.layout_offset_x, job.layout.layout_offset_y);
    preview_view_->setGraphicDragTarget(preview_graphic_group_);

    bounds = bounds.united(preview_graphic_group_->sceneBoundingRect());
    scene_->setSceneRect(bounds.adjusted(-40, -40, 40, 40));
    if (preview_view_)
        preview_view_->fitAll();
    updateGraphicSizeLabels();
    rebuildLivePlotScene();
}

void MainWindow::rebuildLivePlotScene()
{
    if (!live_scene_)
        return;

    live_scene_->clear();
    live_trail_item_ = nullptr;
    live_move_path_item_ = nullptr;
    live_job_path_item_ = nullptr;
    live_material_item_ = nullptr;

    const PlotJobSettings job = collectJobSettings();
    live_scene_->addPath(deviceAreaPath(job.material),
                         QPen(QColor(235, 194, 194), 0, Qt::DashLine));
    live_material_item_ =
        live_scene_->addPath(materialOutlinePath(job.material), QPen(Qt::black, 0, Qt::SolidLine));
    live_scene_->addPath(materialAvailableAreaPath(job.material),
                         QPen(Qt::black, 0, Qt::DashLine));

    if (!current_file_.isEmpty()) {
        QPainterPath path;
        QString err;
        if (loadCurrentDesign(path, err)) {
            const QPainterPath job_path =
                design_is_filled_trace_ ? processFilledTracePath(path, job, activePlugin())
                                        : processJobPath(path, job, activePlugin());
            if (design_is_filled_trace_) {
                QPainterPath filled = job_path;
                filled.setFillRule(Qt::OddEvenFill);
                live_job_path_item_ =
                    live_scene_->addPath(filled, Qt::NoPen, QBrush(QColor(50, 50, 50)));
            } else {
                const MoveCutPaths paths = splitMoveCutPaths(job_path);
                live_move_path_item_ = live_scene_->addPath(paths.move, penPlotMove());
                live_job_path_item_ = live_scene_->addPath(paths.cut, penPlotCut());
            }
        }
    }

    live_scene_->setSceneRect(live_scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40));
    if (live_view_)
        live_view_->fitAll();

    updateLiveStatusBar(0, 0);
}

namespace {

QString formatMinSec(int seconds)
{
    if (seconds <= 0)
        return QString();
    return QStringLiteral("%1 min %2 sec").arg(seconds / 60).arg(seconds % 60);
}

} // namespace

void MainWindow::updateLiveStatusBar(qint64 sent, qint64 total)
{
    if (!live_source_label_)
        return;

    if (current_file_.isEmpty())
        live_source_label_->setText(QStringLiteral("Source: None"));
    else
        live_source_label_->setText(
            QStringLiteral("Source: %1").arg(QFileInfo(current_file_).fileName()));

    if (!current_file_.isEmpty()) {
        QPainterPath path;
        QString err;
        if (loadCurrentDesign(path, err)) {
            const QRectF bb =
                processJobPath(path, last_job_settings_, activePlugin()).boundingRect();
            live_size_label_->setText(QStringLiteral("Size: %1mm x %2mm")
                                          .arg(bb.height(), 0, 'f', 2)
                                          .arg(bb.width(), 0, 'f', 2));
        } else {
            live_size_label_->clear();
        }
    } else {
        live_size_label_->clear();
    }

    if (send_active_ && send_duration_estimate_sec_ > 0) {
        live_duration_label_->setText(
            QStringLiteral("Duration: %1").arg(formatMinSec(send_duration_estimate_sec_)));

        const qint64 left_sec =
            send_estimated_end_.isValid()
                ? qMax<qint64>(0, QDateTime::currentDateTime().secsTo(send_estimated_end_))
                : 0;
        if (left_sec > 0) {
            live_left_label_->setText(QStringLiteral("Left: %1").arg(formatMinSec(int(left_sec))));
            live_eta_label_->setText(
                QStringLiteral("Eta: %1").arg(send_estimated_end_.toString(QStringLiteral("HH:mm:ss"))));
        } else if (sent > 0 && total > 0 && send_started_.isValid()) {
            const qint64 elapsed = qMax<qint64>(1, send_started_.secsTo(QDateTime::currentDateTime()));
            const qint64 remain = qMax<qint64>(0, total - sent);
            const qint64 est = (remain * elapsed) / sent;
            live_left_label_->setText(QStringLiteral("Left: %1").arg(formatMinSec(int(est))));
            live_eta_label_->setText(QStringLiteral("Eta: %1")
                                         .arg(QDateTime::currentDateTime()
                                                  .addSecs(int(est))
                                                  .toString(QStringLiteral("HH:mm:ss"))));
        } else {
            live_left_label_->clear();
            live_eta_label_->clear();
        }
    } else {
        live_duration_label_->clear();
        live_left_label_->clear();
        live_eta_label_->clear();
    }

    if (live_progress_bar_ && total > 0)
        live_progress_bar_->setValue(int((100 * sent) / total));
}

void MainWindow::updateLiveStatusTick()
{
    if (!send_active_)
        return;
    updateLiveStatusBar(send_progress_sent_, send_total_bytes_);
}

void MainWindow::showLivePlotContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    if (send_active_)
        menu.addAction(trInk("Przerwij zadanie"), this, &MainWindow::onSendCancel);
    if (!menu.isEmpty())
        menu.addSeparator();
    menu.addAction(trInk("Pokaż całość"), this, &MainWindow::onLiveFitAll);
    menu.addAction(trInk("Wyczyść wykres"), this, &MainWindow::onLiveClearPlot);
    if (live_view_)
        menu.exec(live_view_->viewport()->mapToGlobal(pos));
}

void MainWindow::onLiveFitAll()
{
    if (live_view_)
        live_view_->fitAll();
}

void MainWindow::onLiveActionClicked()
{
    if (send_active_) {
        onSendPauseResume();
        return;
    }
    onSend();
}

void MainWindow::onLiveClearPlot()
{
    if (!live_scene_)
        return;
    if (live_trail_item_) {
        live_scene_->removeItem(live_trail_item_);
        delete live_trail_item_;
        live_trail_item_ = nullptr;
    }
}

void MainWindow::onOpenSvg()
{
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("SVG lub DXF"), QStringLiteral(),
                                     QStringLiteral("Wektory (*.svg *.SVG *.dxf *.DXF);;Wszystkie (*)"));
    if (path.isEmpty())
        return;
    applyOpenedDesign(path);
}

void MainWindow::onImportBitmap()
{
    if (!isBitmapTracingAvailable()) {
        QMessageBox::information(
            this, QStringLiteral("Inkcut"),
            QStringLiteral("Import bitmapy wymaga libpotrace (np. pakiet libpotrace-dev) i przebudowy."));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import bitmapy"), QStringLiteral(),
        QStringLiteral("Obrazy (*.png *.PNG *.bmp *.BMP *.jpg *.jpeg);;Wszystkie (*)"));
    if (path.isEmpty())
        return;

    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             trInk("Nie można wczytać obrazu."));
        return;
    }

    BitmapTraceOptions trace_opts;
    if (!runBitmapTraceDialog(this, img, trace_opts))
        return;

    QProgressDialog progress(trInk("Wektoryzacja bitmapy…"), QString(), 0, 100, this);
    progress.setWindowTitle(QStringLiteral("Inkcut"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    progress.setCancelButton(nullptr);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QString svg_xml;
    QString terr;
    const auto report_progress = [&progress](int pct) {
        progress.setValue(pct);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };

    if (!traceBitmapToSvg(img, svg_xml, &terr, report_progress, trace_opts)) {
        progress.close();
        QMessageBox::warning(this, QStringLiteral("Inkcut"),
                             terr.isEmpty() ? trInk("Potrace nie zwrócił ścieżki.") : terr);
        return;
    }
    progress.setValue(100);
    progress.close();

    current_file_ = path;
    current_svg_xml_ = svg_xml;
    design_is_filled_trace_ = true;

    QString err;
    QStringList warns;
    QPainterPath path_model;
    if (!loadCurrentDesign(path_model, err, &warns)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);
        return;
    }

    PlotJobSettings job = collectJobSettings();
    applyJobSettingsToUi(job);
    pushRecentPath(path);
    rebuildRecentMenu();
    refreshLayerAndColorLists();
    rebuildPreview();
    updateFilePathLabel();
    showSvgWarningsIfAny(warns);
}

void MainWindow::onExportProgram()
{
    if (current_file_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 QStringLiteral("Najpierw wczytaj plik SVG lub DXF."));
        return;
    }

    const QString out_path =
        QFileDialog::getSaveFileName(this, trInk("Zapisz"), QStringLiteral("job.hpgl"),
                                     QStringLiteral("Program (*.hpgl *.plt *.txt);;Wszystkie (*)"));
    if (out_path.isEmpty())
        return;

    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);
        return;
    }

    const std::string program = buildPlotProgram(path, collectJobSettings(), activePlugin(),
                                               design_is_filled_trace_);
    QFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), trInk("Zapis nie powiódł się."));
        return;
    }
    out.write(program.data(), static_cast<qint64>(program.size()));
}

void MainWindow::onExportJobJson()
{
    if (current_file_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 QStringLiteral("Najpierw wczytaj plik SVG lub DXF."));
        return;
    }

    const QString out_path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Eksport JSON"), QStringLiteral("job.json"),
                                     QStringLiteral("JSON (*.json);;Wszystkie (*)"));
    if (out_path.isEmpty())
        return;

    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);
        return;
    }

    const QString kind =
        current_file_.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive) ? QStringLiteral("dxf")
                                                                         : QStringLiteral("svg");
    const QString json =
        exportJobDocumentToJson(QFileInfo(current_file_).absoluteFilePath(), kind,
                                path.boundingRect(), collectJobSettings());

    QFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), trInk("Zapis nie powiódł się."));
        return;
    }
    out.write(json.toUtf8());
}

void MainWindow::onSend()
{
    if (monitor_live_chk_->isChecked()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 trInk("Wyłącz „Nasłuch na żywo”, aby zwolnić port na wysyłkę."));
        return;
    }

    stopControlConnection();

    if (current_file_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Inkcut"),
                                 QStringLiteral("Najpierw wczytaj plik SVG lub DXF."));
        return;
    }

    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err)) {
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);
        return;
    }

    last_job_settings_ = collectJobSettings();
    const QPainterPath model_path =
        design_is_filled_trace_ ? processFilledTracePath(path, last_job_settings_, activePlugin())
                                : processJobPath(path, last_job_settings_, activePlugin());

    if (!confirmSendApproval(last_job_settings_, model_path))
        return;

    savePersistedSettings();
    recordJobHistory(JobRunStatus::Approved, 0, QString());

    rebuildLivePlotScene();
    send_active_ = true;
    send_started_ = QDateTime::currentDateTime();
    send_total_bytes_ = 0;
    send_paused_ = false;
    send_duration_estimate_sec_ = 0;
    send_estimated_end_ = QDateTime();

    const std::string program = buildPlotProgram(path, last_job_settings_, activePlugin(),
                                               design_is_filled_trace_);
    const QByteArray payload = QByteArray::fromStdString(program);
    send_total_bytes_ = payload.size();

    if (last_job_settings_.velocity > 0) {
        const double len = model_path.length();
        send_duration_estimate_sec_ =
            int(qMax(1.0, len / double(last_job_settings_.velocity)));
    } else if (send_total_bytes_ > 0) {
        send_duration_estimate_sec_ = int(qMax(1.0, send_total_bytes_ / 1200.0));
    }
    if (send_duration_estimate_sec_ > 0)
        send_estimated_end_ = send_started_.addSecs(send_duration_estimate_sec_);

    if (live_action_btn_) {
        applyLiveActionUi(UiIcons::LiveAction::Pause);
        live_action_btn_->setEnabled(true);
    }
    if (live_abort_btn_)
        live_abort_btn_->setVisible(true);
    if (live_progress_bar_)
        live_progress_bar_->setValue(0);
    if (live_status_timer_)
        live_status_timer_->start();
    updateLiveStatusBar(0, send_total_bytes_);

    if (last_job_settings_.device.transport != PlotTransportKind::SerialPort) {
        const TransportResult tr = sendPlotPayload(payload, last_job_settings_.device);
        if (monitor_log_send_chk_->isChecked())
            appendMonitorRaw(payload, true);
        send_active_ = false;
        if (live_status_timer_)
            live_status_timer_->stop();
        if (live_abort_btn_)
            live_abort_btn_->setVisible(false);
        if (live_action_btn_)
            applyLiveActionUi(UiIcons::LiveAction::Start);
        if (live_progress_bar_)
            live_progress_bar_->setValue(tr.ok ? 100 : 0);
        updateLiveStatusBar(tr.bytes_written, send_total_bytes_);
        recordJobHistory(tr.ok ? JobRunStatus::Complete : JobRunStatus::Error, tr.bytes_written,
                         tr.error_message);
        if (tr.ok)
            QMessageBox::information(this, QStringLiteral("Inkcut"), trInk("Zapisano."));
        else
            QMessageBox::warning(this, QStringLiteral("Inkcut"), tr.error_message);
        return;
    }

    stopSendWorker();
    if (monitor_log_send_chk_->isChecked())
        appendMonitorRaw(payload, true);

    send_worker_ = new PlotSendWorker();
    send_worker_->setPayload(payload);
    send_worker_->setDevice(last_job_settings_.device);
    send_worker_->setLiveParse(last_job_settings_.protocol.protocol,
                               last_job_settings_.protocol.plot_scale);

    send_thread_ = std::make_unique<QThread>();
    send_worker_->moveToThread(send_thread_.get());
    connect(send_thread_.get(), &QThread::started, send_worker_, &PlotSendWorker::run);
    connect(send_worker_, &PlotSendWorker::progress, this, &MainWindow::onSendProgress);
    connect(send_worker_, &PlotSendWorker::livePosition, this, &MainWindow::onSendLivePosition);
    connect(send_worker_, &PlotSendWorker::finished, this, &MainWindow::onSendFinished);
    connect(send_worker_, &PlotSendWorker::finished, send_thread_.get(), &QThread::quit);

    if (bottom_tabs_) {
        for (int i = 0; i < bottom_tabs_->count(); ++i) {
            if (bottom_tabs_->tabText(i) == QStringLiteral("Live")) {
                bottom_tabs_->setCurrentIndex(i);
                break;
            }
        }
    }

    recordJobHistory(JobRunStatus::Running, 0, {});
    send_thread_->start();
}

void MainWindow::showSvgWarningsIfAny(const QStringList& warnings)
{
    if (warnings.isEmpty())
        return;
    QMessageBox::information(this, trInk("SVG — ostrzeżenia"),
                             warnings.join(QLatin1Char('\n')));
}

void MainWindow::ingestRxPlotterHints(const QByteArray& rx)
{
    if (!plot_position_label_)
        return;
    rx_accum_.append(rx);
    if (rx_accum_.size() > 8192)
        rx_accum_.remove(0, rx_accum_.size() - 4096);

    const PlotJobSettings job = collectJobSettings();
    if (const auto pos = parseLivePositionFromRx(rx_accum_, job.protocol.protocol,
                                                 job.protocol.plot_scale)) {
        plot_position_label_->setText(QStringLiteral("Pozycja (RX): %1, %2")
                                          .arg(pos->x, 0, 'f', 3)
                                          .arg(pos->y, 0, 'f', 3));
    }
}

void MainWindow::refreshDevicePlugins()
{
    if (!plugin_combo_ || !plugin_loader_)
        return;
    QStringList errs;
    plugin_loader_->rescan(&errs);
    {
        const QSignalBlocker b(plugin_combo_);
        plugin_combo_->clear();
        plugin_combo_->addItem(trInk("— ręczny port —"), QString());
        for (DevicePlugin* p : plugin_loader_->plugins())
            plugin_combo_->addItem(p->displayName(), p->pluginId());
    }
    for (const QString& e : errs)
        appendMonitorRaw((e + QLatin1Char('\n')).toUtf8(), false);
}

void MainWindow::onPluginComboChanged(int idx)
{
    if (idx <= 0 || !plugin_loader_)
        return;
    const QString pid = plugin_combo_->itemData(idx).toString();
    DevicePlugin* plug = nullptr;
    for (DevicePlugin* p : plugin_loader_->plugins()) {
        if (p->pluginId() == pid) {
            plug = p;
            break;
        }
    }
    if (!plug)
        return;
    PlotJobSettings job = collectJobSettings();
    job.plugin_id = pid;
    plug->configureJobDefaults(job);
    applyJobSettingsToUi(job);
    pipeline_cache_.plugin_id = job.plugin_id;
    savePersistedSettings();
    rebuildPreview();
    const auto ports = plug->probeSerialPorts();
    if (!ports.isEmpty())
        port_edit_->setText(ports.front().port_name);
}

namespace {

QString jobStatusLabel(JobRunStatus status)
{
    switch (status) {
    case JobRunStatus::Approved:
        return QStringLiteral("zatwierdzono");
    case JobRunStatus::Running:
        return QStringLiteral("w toku");
    case JobRunStatus::Paused:
        return QStringLiteral("pauza");
    case JobRunStatus::Complete:
        return trInk("ukończono");
    case JobRunStatus::Cancelled:
        return QStringLiteral("anulowano");
    case JobRunStatus::Error:
        return trInk("błąd");
    case JobRunStatus::Staged:
        return QStringLiteral("przygotowane");
    default:
        return QStringLiteral("—");
    }
}

QString formatJobDuration(int seconds)
{
    if (seconds <= 0)
        return QString();
    return QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
}

QString formatJobDate(const QString& iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        return iso;
    return QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat);
}

} // namespace

void MainWindow::recordJobHistory(JobRunStatus status, qint64 bytes_sent, const QString& err)
{
    const auto fillEntry = [this](JobHistoryEntry& e) {
        e.source_path = QFileInfo(current_file_).absoluteFilePath();
        e.source_kind =
            current_file_.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive)
                ? QStringLiteral("dxf")
                : QStringLiteral("svg");
        e.settings = last_job_settings_;
        QPainterPath p;
        QString pe;
        if (loadCurrentDesign(p, pe))
            e.bounds = processJobPath(p, e.settings, nullptr).boundingRect();
    };

    if (status == JobRunStatus::Approved) {
        JobHistoryEntry e;
        e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        e.time_iso = now;
        e.started_iso = now;
        e.status = status;
        e.bytes_sent = bytes_sent;
        e.error_message = err;
        fillEntry(e);
        appendJobHistoryEntry(e);
        active_history_id_ = e.id;
        rebuildJobHistoryUi();
        return;
    }

    if (!active_history_id_.isEmpty()) {
        QVector<JobHistoryEntry> list = loadJobHistory();
        for (JobHistoryEntry& e : list) {
            if (e.id != active_history_id_)
                continue;
            e.status = status;
            e.bytes_sent = bytes_sent;
            if (!err.isEmpty())
                e.error_message = err;
            if (status == JobRunStatus::Running && e.started_iso.isEmpty())
                e.started_iso = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            if (status == JobRunStatus::Complete || status == JobRunStatus::Error
                || status == JobRunStatus::Cancelled) {
                e.ended_iso = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
                const QDateTime start =
                    QDateTime::fromString(e.started_iso.isEmpty() ? e.time_iso : e.started_iso,
                                          Qt::ISODateWithMs);
                const QDateTime end = QDateTime::fromString(e.ended_iso, Qt::ISODateWithMs);
                if (start.isValid() && end.isValid())
                    e.duration_sec = int(start.secsTo(end));
                if (status == JobRunStatus::Complete)
                    e.run_count += 1;
                active_history_id_.clear();
            }
            saveJobHistory(list);
            rebuildJobHistoryUi();
            return;
        }
    }

    JobHistoryEntry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.time_iso = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    e.status = status;
    e.bytes_sent = bytes_sent;
    e.error_message = err;
    fillEntry(e);
    appendJobHistoryEntry(e);
    rebuildJobHistoryUi();
}

void MainWindow::rebuildJobHistoryUi()
{
    if (!history_table_)
        return;
    history_table_->setRowCount(0);

    const QVector<JobHistoryEntry> entries = loadJobHistory();
    for (const JobHistoryEntry& e : entries) {
        const int row = history_table_->rowCount();
        history_table_->insertRow(row);

        auto* date_item = new QTableWidgetItem(formatJobDate(e.time_iso));
        date_item->setData(Qt::UserRole, e.source_path);
        date_item->setData(Qt::UserRole + 1, e.id);
        date_item->setToolTip(e.source_path);
        history_table_->setItem(row, 0, date_item);

        auto* doc_item = new QTableWidgetItem(QFileInfo(e.source_path).fileName());
        doc_item->setToolTip(e.source_path);
        history_table_->setItem(row, 1, doc_item);

        history_table_->setItem(row, 2,
                                new QTableWidgetItem(e.run_count > 0 ? QString::number(e.run_count)
                                                                     : QStringLiteral("0")));

        history_table_->setItem(row, 3, new QTableWidgetItem(formatJobDuration(e.duration_sec)));

        auto* status_item = new QTableWidgetItem(jobStatusLabel(e.status));
        if (!e.error_message.isEmpty())
            status_item->setToolTip(e.error_message);
        history_table_->setItem(row, 4, status_item);

        history_table_->setItem(
            row, 5, new QTableWidgetItem(QString::number(e.settings.layout.copies)));

        history_table_->setItem(
            row, 6,
            new QTableWidgetItem(QStringLiteral("%1 °").arg(e.settings.layout.rotation_deg, 0, 'f', 2)));

        QString size_txt = QStringLiteral("—");
        if (e.bounds.isValid() && (e.bounds.width() > 0 || e.bounds.height() > 0)) {
            size_txt = QStringLiteral("%1 × %2 mm")
                           .arg(e.bounds.height(), 0, 'f', 1)
                           .arg(e.bounds.width(), 0, 'f', 1);
        }
        history_table_->setItem(row, 7, new QTableWidgetItem(size_txt));

        history_table_->setItem(
            row, 8,
            new QTableWidgetItem(QStringLiteral("%1 × %2 mm")
                                     .arg(e.settings.material.width, 0, 'f', 1)
                                     .arg(e.settings.material.height, 0, 'f', 1)));
    }
    history_table_->resizeColumnsToContents();
}

void MainWindow::openJobHistoryRow(int row)
{
    if (!history_table_ || row < 0 || row >= history_table_->rowCount())
        return;
    const QTableWidgetItem* date_item = history_table_->item(row, 0);
    if (!date_item)
        return;
    const QString path = date_item->data(Qt::UserRole).toString();
    const QString hist_id = date_item->data(Qt::UserRole + 1).toString();
    for (const JobHistoryEntry& e : loadJobHistory()) {
        if (e.id == hist_id) {
            applyJobSettingsToUi(e.settings);
            break;
        }
    }
    if (!path.isEmpty())
        applyOpenedDesign(path);
}

void MainWindow::removeJobHistoryRow(int row)
{
    if (!history_table_ || row < 0 || row >= history_table_->rowCount())
        return;
    const QTableWidgetItem* date_item = history_table_->item(row, 0);
    if (!date_item)
        return;
    const QString hist_id = date_item->data(Qt::UserRole + 1).toString();
    if (hist_id == active_history_id_)
        active_history_id_.clear();
    removeJobHistoryEntry(hist_id);
    rebuildJobHistoryUi();
}

void MainWindow::showJobHistoryContextMenu(const QPoint& pos)
{
    if (!history_table_)
        return;
    const int row = history_table_->indexAt(pos).row();
    if (row < 0)
        return;

    QMenu menu(this);
    menu.addAction(trInk("Otwórz zadanie"), this, [this, row]() { openJobHistoryRow(row); });
    menu.addAction(trInk("Wyślij na urządzenie"), this, [this, row]() {
        openJobHistoryRow(row);
        onSend();
    });
    menu.addSeparator();
    menu.addAction(trInk("Usuń z listy"), this, [this, row]() { removeJobHistoryRow(row); });
    menu.exec(history_table_->viewport()->mapToGlobal(pos));
}

void MainWindow::onOpenDeviceSetup()
{
    PlotJobSettings job = collectJobSettings();
    if (!runDeviceSetupDialog(this, job, plugin_loader_.get()))
        return;
    applyJobSettingsToUi(job);
    pipeline_cache_.plugin_id = job.plugin_id;
    savePersistedSettings();
    rebuildPreview();
}

void MainWindow::onOpenSettings()
{
    AppSettings app = app_settings_;
    PlotJobSettings job = collectJobSettings();
    app.flatten_step = job.flatten_step;
    if (!runSettingsDialog(this, app, job))
        return;
    app_settings_ = app;
    job.flatten_step = app.flatten_step;
    applyJobSettingsToUi(job);
    applyApplicationTheme(*qApp, app_settings_.dock_style);
    installInkcutTranslator(*qApp, app_settings_.language);
    applyAppSettingsToUi();
    applyUiProfile();
    retranslateUi();
    saveAppSettings(app_settings_);
    savePersistedSettings();
    rebuildPreview();
}

void MainWindow::applyAppSettingsToUi()
{
    pipeline_cache_.flatten_step = app_settings_.flatten_step;
    updateUnitSuffixes();

    auto apply_grid = [this](LivePlotView* view) {
        if (!view)
            return;
        view->setGridVisible(app_settings_.show_grid_x || app_settings_.show_grid_y);
        view->setGridAxes(app_settings_.show_grid_x, app_settings_.show_grid_y);
        view->setGridAlpha(app_settings_.grid_alpha);
    };
    apply_grid(preview_view_);
    apply_grid(live_view_);
}

void MainWindow::applyUiProfile()
{
    const UiProfileMetrics m = metricsFor(app_settings_.ui_profile);
    const bool tablet = app_settings_.ui_profile == UiProfile::Tablet;

    setStyleSheet(profileStyleSheet(app_settings_.ui_profile));

    if (preview_view_)
        preview_view_->setMinimumSize(m.plot_view_minimum);
    if (live_view_)
        live_view_->setMinimumSize(m.plot_view_minimum);

    if (QDockWidget* dock = findChild<QDockWidget*>(QStringLiteral("dock_left")))
        dock->setMinimumWidth(m.left_dock_minimum_width);

    if (control_grid_)
        control_grid_->setSpacing(m.control_grid_spacing);

    for (QPushButton* btn : findChildren<QPushButton*>()) {
        if (btn->objectName() == QLatin1String("plot_zoom_btn")) {
            btn->setMinimumSize(m.zoom_button_min_px, m.zoom_button_min_px);
            btn->setMaximumSize(m.zoom_button_min_px, m.zoom_button_min_px);
        }
    }

    if (control_tab_) {
        for (QPushButton* btn : control_tab_->findChildren<QPushButton*>()) {
            btn->setMinimumSize(m.control_button_min_px, m.control_button_min_px);
            btn->setIconSize(QSize(m.control_icon_px, m.control_icon_px));
        }
    }
    if (control_load_btn_)
        control_load_btn_->setMinimumHeight(m.control_button_min_px);
    if (control_unload_btn_)
        control_unload_btn_->setMinimumHeight(m.control_button_min_px);
    if (control_step_spin_)
        control_step_spin_->setMinimumHeight(tablet ? 36 : 0);

    const int icon_px = tablet ? 26 : 22;
    for (QPushButton* btn :
         {live_action_btn_, live_abort_btn_, add_stack_btn_, remove_stack_btn_, plugin_refresh_btn_}) {
        if (btn)
            btn->setIconSize(QSize(icon_px, icon_px));
    }

    if (bottom_dock_)
        resizeDocks({bottom_dock_}, {m.bottom_dock_height}, Qt::Vertical);

    if (tablet) {
        setMinimumSize(800, 480);
        showMaximized();
    } else {
        setMinimumSize(960, 640);
        if (isMaximized())
            showNormal();
        resize(m.default_window);
    }

    if (preview_view_ && scene_ && !scene_->items().isEmpty())
        preview_view_->fitAll();
    if (live_view_ && live_scene_ && !live_scene_->items().isEmpty())
        live_view_->fitAll();
}

void MainWindow::onLayerOrColorFilterChanged()
{
    rebuildPreview();
}

void MainWindow::applyJobSettingsToUi(const PlotJobSettings& job)
{
    pipeline_cache_ = job;

    const QSignalBlocker b_order(order_combo_);
    const QSignalBlocker b_preset(preset_combo_);
    const QSignalBlocker b_transport(transport_combo_);

    if (order_combo_)
        order_combo_->setCurrentIndex(order_combo_->findData(int(job.order)));

    const QString& u = app_settings_.units;
    mat_w_spin_->setValue(mmToDisplay(job.material.width, u));
    mat_h_spin_->setValue(mmToDisplay(job.material.height, u));
    mat_pad_l_spin_->setValue(mmToDisplay(job.material.padding_left, u));
    mat_pad_t_spin_->setValue(mmToDisplay(job.material.padding_top, u));
    mat_pad_r_spin_->setValue(mmToDisplay(job.material.padding_right, u));
    mat_pad_b_spin_->setValue(mmToDisplay(job.material.padding_bottom, u));
    if (mat_roll_chk_)
        mat_roll_chk_->setChecked(job.material.is_roll);
    if (mat_force_speed_chk_)
        mat_force_speed_chk_->setChecked(job.material.use_custom_force_speed);
    if (mat_force_spin_)
        mat_force_spin_->setValue(job.material.force);
    if (mat_speed_spin_)
        mat_speed_spin_->setValue(job.material.speed);

    auto_shift_chk_->setChecked(job.layout.auto_shift);
    align_center_x_chk_->setChecked(job.layout.align_center_x);
    align_center_y_chk_->setChecked(job.layout.align_center_y);

    if (job.feed_to_end) {
        feed_after_rb_->setChecked(true);
        feed_return_rb_->setChecked(false);
    } else {
        feed_return_rb_->setChecked(true);
        feed_after_rb_->setChecked(false);
    }
    feed_after_spin_->setValue(mmToDisplay(job.feed_after, u));

    scale_x_spin_->setValue(job.layout.scale_x);
    scale_y_spin_->setValue(job.layout.scale_y);
    scale_pct_x_spin_->setValue(job.layout.scale_x * 100.0);
    scale_pct_y_spin_->setValue(job.layout.scale_y * 100.0);
    rotation_spin_->setValue(job.layout.rotation_deg);
    copies_spin_->setValue(job.layout.copies);
    copy_gap_x_spin_->setValue(mmToDisplay(job.layout.copy_spacing_x, u));
    copy_gap_y_spin_->setValue(mmToDisplay(job.layout.copy_spacing_y, u));
    mirror_x_chk_->setChecked(job.layout.mirror_x);
    mirror_y_chk_->setChecked(job.layout.mirror_y);
    auto_rotate_chk_->setChecked(job.layout.auto_rotate);
    auto_copies_chk_->setChecked(job.layout.auto_copies);
    if (auto_scale_chk_)
        auto_scale_chk_->setChecked(job.layout.auto_scale);
    lock_scale_chk_->setChecked(job.layout.lock_scale);
    if (layout_offset_x_spin_) {
        QSignalBlocker bx(layout_offset_x_spin_);
        layout_offset_x_spin_->setValue(job.layout.layout_offset_x);
    }
    if (layout_offset_y_spin_) {
        QSignalBlocker by(layout_offset_y_spin_);
        layout_offset_y_spin_->setValue(job.layout.layout_offset_y);
    }

    plot_weedline_chk_->setChecked(job.weedlines.plot_weedline);
    copy_weedline_chk_->setChecked(job.weedlines.copy_weedline);
    if (plot_pad_l_spin_) {
        plot_pad_l_spin_->setValue(mmToDisplay(job.weedlines.plot_pad_left, u));
        plot_pad_t_spin_->setValue(mmToDisplay(job.weedlines.plot_pad_top, u));
        plot_pad_r_spin_->setValue(mmToDisplay(job.weedlines.plot_pad_right, u));
        plot_pad_b_spin_->setValue(mmToDisplay(job.weedlines.plot_pad_bottom, u));
    }
    copy_pad_l_spin_->setValue(mmToDisplay(job.weedlines.copy_pad_left, u));
    copy_pad_t_spin_->setValue(mmToDisplay(job.weedlines.copy_pad_top, u));
    copy_pad_r_spin_->setValue(mmToDisplay(job.weedlines.copy_pad_right, u));
    copy_pad_b_spin_->setValue(mmToDisplay(job.weedlines.copy_pad_bottom, u));

    if (plugin_combo_ && !job.plugin_id.isEmpty()) {
        const int pi = plugin_combo_->findData(job.plugin_id);
        if (pi >= 0)
            plugin_combo_->setCurrentIndex(pi);
    }

    preset_combo_->setCurrentIndex(
        std::max(0, preset_combo_->findData(job.device.preset_id)));
    PlotTransportKind transport = job.device.transport;
    if (transport == PlotTransportKind::Printer)
        transport = PlotTransportKind::SerialPort;
    const int tidx = transport_combo_->findData(int(transport));
    transport_combo_->setCurrentIndex(tidx >= 0 ? tidx : 0);
    port_edit_->setText(job.device.port_name);
    baud_spin_->setValue(job.device.baud_rate);
    output_path_edit_->setText(job.device.output_path);
    if (printer_edit_)
        printer_edit_->setText(job.device.printer_name);

    active_device_ = job.device;
    onTransportChanged(0);

    if (!current_svg_xml_.isEmpty())
        refreshLayerAndColorLists();
}

void MainWindow::loadPersistedSettings()
{
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    pipeline_cache_.flatten_step = app_settings_.flatten_step;

    const QByteArray raw = settings.value(QStringLiteral("last_job_settings")).toByteArray();
    if (raw.isEmpty())
        return;
    PlotJobSettings job;
    if (plotJobSettingsFromJsonString(raw, job, nullptr))
        applyJobSettingsToUi(job);
}

void MainWindow::savePersistedSettings()
{
    QSettings settings(QStringLiteral("inkcut"), QStringLiteral("gui"));
    PlotJobSettings job = collectJobSettings();
    job.flatten_step = app_settings_.flatten_step;
    settings.setValue(QStringLiteral("last_job_settings"), plotJobSettingsToJsonString(job).toUtf8());
    saveAppSettings(app_settings_);
}

void MainWindow::onPresetChanged(int idx)
{
    DevicePreset p;
    if (!devicePresetById(preset_combo_->itemData(idx).toString(), p))
        return;
    applyPresetToUi(p);
    savePersistedSettings();
    rebuildPreview();
}

void MainWindow::applyPresetToUi(const DevicePreset& preset)
{
    PlotJobSettings job = collectJobSettings();
    applyPresetToJob(preset, job);
    applyJobSettingsToUi(job);
}

void MainWindow::onTransportChanged(int)
{
    const auto kind =
        static_cast<PlotTransportKind>(transport_combo_->currentData().toInt());
    const bool file_out = kind == PlotTransportKind::FileOutput;
    if (output_path_edit_)
        output_path_edit_->setVisible(file_out);
    port_edit_->setEnabled(kind == PlotTransportKind::SerialPort);
    baud_spin_->setEnabled(kind == PlotTransportKind::SerialPort);
}

void MainWindow::onLayoutChanged()
{
    rebuildPreview();
}

void MainWindow::addPassFilterListRow(QListWidget* list, const QString& storage_key,
                                      const QString& label, bool enabled, int pass_count,
                                      int row_height)
{
    auto* it = new QListWidgetItem();
    it->setData(Qt::UserRole, storage_key);
    list->addItem(it);

    auto* row = new QWidget(list);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(4);

    auto* chk = new QCheckBox(label, row);
    const QFont row_font = chk->font();
    const QFontMetrics fm(row_font);
    chk->setChecked(enabled);

    const int ctrl_h = metricsFor(app_settings_.ui_profile).filter_pass_button_px;
    const int spin_w = qMax(24, fm.horizontalAdvance(QStringLiteral("99")) + 6);

    auto* dec_btn = new QPushButton(QStringLiteral("−"), row);
    dec_btn->setObjectName(QStringLiteral("filter_pass_dec"));
    dec_btn->setFont(row_font);
    dec_btn->setFixedSize(ctrl_h, ctrl_h);
    dec_btn->setToolTip(trInk("Zmniejsz liczbę przejść"));

    auto* spin = new QSpinBox(row);
    spin->setObjectName(QStringLiteral("filter_pass_spin"));
    spin->setFont(row_font);
    spin->setRange(1, 99);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    spin->setFixedSize(spin_w, ctrl_h);
    spin->setToolTip(trInk("Liczba przejść plotera (np. twardy materiał)"));
    {
        QSignalBlocker b(spin);
        spin->setValue(pass_count);
    }

    auto* inc_btn = new QPushButton(QStringLiteral("+"), row);
    inc_btn->setObjectName(QStringLiteral("filter_pass_inc"));
    inc_btn->setFont(row_font);
    inc_btn->setFixedSize(ctrl_h, ctrl_h);
    inc_btn->setToolTip(trInk("Zwiększ liczbę przejść"));

    lay->addWidget(chk, 1);
    lay->addWidget(dec_btn);
    lay->addWidget(spin);
    lay->addWidget(inc_btn);

    const int item_h = lay->contentsMargins().top() + lay->contentsMargins().bottom() + ctrl_h + 4;
    it->setSizeHint(QSize(0, qMax(row_height, item_h)));
    row->setMinimumHeight(item_h);

    list->setItemWidget(it, row);

    connect(chk, &QCheckBox::toggled, this, &MainWindow::onLayerOrColorFilterChanged);
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
            &MainWindow::onLayerOrColorFilterChanged);
    connect(dec_btn, &QPushButton::clicked, spin, [spin]() {
        spin->setValue(spin->value() - 1);
    });
    connect(inc_btn, &QPushButton::clicked, spin, [spin]() {
        spin->setValue(spin->value() + 1);
    });
}

void MainWindow::refreshLayerAndColorLists()
{
    if (!layer_list_)
        return;

    const bool is_dxf =
        !current_file_.isEmpty() && current_file_.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive);
    if (dxf_layers_hint_) {
        dxf_layers_hint_->setVisible(is_dxf);
        if (is_dxf) {
            dxf_layers_hint_->setText(
                trInk("DXF: jedna warstwa „Cały dokument”. Filtry kolorów tylko dla SVG."));
        }
    }
    if (is_dxf) {
        const int row_h = metricsFor(app_settings_.ui_profile).filter_list_row_height;
        layer_list_->clear();
        fill_color_list_->clear();
        stroke_color_list_->clear();
        LayerFilterEntry whole;
        whole.layer_id.clear();
        whole.name = trInk("Cały dokument");
        if (!pipeline_cache_.layer_filters.isEmpty()) {
            for (const LayerFilterEntry& e : pipeline_cache_.layer_filters) {
                if (e.layer_id.isEmpty()) {
                    whole.enabled = e.enabled;
                    whole.pass_count = std::max(1, e.pass_count);
                    break;
                }
            }
        }
        addPassFilterListRow(layer_list_, whole.layer_id, whole.name, whole.enabled,
                             whole.pass_count, row_h);
        return;
    }

    if (current_svg_xml_.isEmpty())
        return;

    const int row_h = metricsFor(app_settings_.ui_profile).filter_list_row_height;

    QHash<QString, LayerFilterEntry> saved;
    for (const LayerFilterEntry& e : pipeline_cache_.layer_filters)
        saved.insert(e.layer_id, e);

    QVector<LayerFilterEntry> rows = discoverSvgLayers(current_svg_xml_);
    if (rows.isEmpty()) {
        LayerFilterEntry whole;
        whole.layer_id.clear();
        whole.name = trInk("Cały dokument");
        whole.enabled = true;
        whole.pass_count = 1;
        if (saved.contains(whole.layer_id)) {
            const LayerFilterEntry prev = saved.value(whole.layer_id);
            whole.enabled = prev.enabled;
            whole.pass_count = qMax(1, prev.pass_count);
        }
        rows.push_back(whole);
    }

    layer_list_->clear();
    for (LayerFilterEntry e : rows) {
        if (saved.contains(e.layer_id)) {
            const LayerFilterEntry prev = saved.value(e.layer_id);
            e.enabled = prev.enabled;
            e.pass_count = qMax(1, prev.pass_count);
        }
        addPassFilterListRow(layer_list_, e.layer_id, e.name, e.enabled, e.pass_count, row_h);
    }

    QHash<QString, ColorFilterEntry> saved_colors;
    for (const ColorFilterEntry& c : pipeline_cache_.color_filters)
        saved_colors.insert(colorFilterStorageKey(c.color_key, c.is_fill), c);

    const QVector<ColorFilterEntry> colors = discoverSvgColors(current_svg_xml_);
    fill_color_list_->clear();
    stroke_color_list_->clear();
    for (ColorFilterEntry c : colors) {
        const QString key = colorFilterStorageKey(c.color_key, c.is_fill);
        if (saved_colors.contains(key)) {
            const ColorFilterEntry prev = saved_colors.value(key);
            c.enabled = prev.enabled;
            c.pass_count = qMax(1, prev.pass_count);
        }
        if (c.is_fill)
            addPassFilterListRow(fill_color_list_, c.color_key, c.color_key, c.enabled,
                                 c.pass_count, row_h);
        else
            addPassFilterListRow(stroke_color_list_, c.color_key, c.color_key, c.enabled,
                                 c.pass_count, row_h);
    }
}

void MainWindow::updateGraphicSizeLabels()
{
    if (!graphic_size_w_label_ || current_file_.isEmpty()) {
        if (graphic_size_w_label_)
            graphic_size_w_label_->setText(QStringLiteral("—"));
        if (graphic_size_h_label_)
            graphic_size_h_label_->setText(QStringLiteral("—"));
        return;
    }
    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err)) {
        graphic_size_w_label_->setText(QStringLiteral("—"));
        graphic_size_h_label_->setText(QStringLiteral("—"));
        return;
    }
    const PlotJobSettings job = collectJobSettings();
    QRectF single;
    applyJobLayout(applyCutOrder(path, job.order), job, &single);
    graphic_size_w_label_->setText(QStringLiteral("%1 mm").arg(single.width(), 0, 'f', 2));
    graphic_size_h_label_->setText(QStringLiteral("%1 mm").arg(single.height(), 0, 'f', 2));
}

void MainWindow::onAddCopyStack()
{
    PlotJobSettings job = collectJobSettings();
    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err))
        return;
    QRectF single;
    applyJobLayout(applyCutOrder(path, job.order), job, &single);
    const int stack_x =
        computeStackCountX(job.material, single, job.layout.copy_spacing_x, job.layout.copy_spacing_y);
    int copies = job.layout.copies;
    applyAddStack(copies, stack_x);
    copies_spin_->setValue(copies);
    onLayoutChanged();
}

void MainWindow::onRemoveCopyStack()
{
    PlotJobSettings job = collectJobSettings();
    QPainterPath path;
    QString err;
    if (!loadCurrentDesign(path, err))
        return;
    QRectF single;
    applyJobLayout(applyCutOrder(path, job.order), job, &single);
    const int stack_x =
        computeStackCountX(job.material, single, job.layout.copy_spacing_x, job.layout.copy_spacing_y);
    int copies = job.layout.copies;
    applyRemoveStack(copies, stack_x);
    copies_spin_->setValue(copies);
    onLayoutChanged();
}

bool MainWindow::confirmSendApproval(const PlotJobSettings& job, const QPainterPath& model_path)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Zatwierdzenie zadania"));
    auto* form = new QFormLayout(&dlg);
    form->addRow(QStringLiteral("Plik"), new QLabel(QFileInfo(current_file_).fileName()));
    form->addRow(trInk("Protokół"),
                 new QLabel(plotProtocolToCli(job.protocol.protocol)));
    form->addRow(trInk("Kopie"), new QLabel(QString::number(job.layout.copies)));
    form->addRow(trInk("Materiał"),
                 new QLabel(QStringLiteral("%1 × %2")
                                .arg(job.material.width)
                                .arg(job.material.height)));
    form->addRow(trInk("Rozmiar cięcia"),
                 new QLabel(QStringLiteral("%1 × %2")
                                .arg(model_path.boundingRect().width(), 0, 'f', 1)
                                .arg(model_path.boundingRect().height(), 0, 'f', 1)));
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);
    return dlg.exec() == QDialog::Accepted;
}

void MainWindow::stopSendWorker()
{
    if (send_worker_)
        send_worker_->requestCancel();
    if (send_thread_ && send_thread_->isRunning()) {
        send_thread_->quit();
        send_thread_->wait(3000);
    }
    send_thread_.reset();
    send_worker_ = nullptr;
    if (live_status_timer_)
        live_status_timer_->stop();
    if (live_action_btn_) {
        applyLiveActionUi(UiIcons::LiveAction::Start);
        live_action_btn_->setEnabled(true);
    }
    if (live_abort_btn_)
        live_abort_btn_->setVisible(false);
}

void MainWindow::onSendPauseResume()
{
    if (!send_worker_)
        return;
    if (send_paused_) {
        send_worker_->requestResume();
        if (live_action_btn_)
            applyLiveActionUi(UiIcons::LiveAction::Pause);
        send_paused_ = false;
    } else {
        send_worker_->requestPause();
        if (live_action_btn_)
            applyLiveActionUi(UiIcons::LiveAction::Resume);
        send_paused_ = true;
    }
}

void MainWindow::onSendCancel()
{
    stopSendWorker();
    recordJobHistory(JobRunStatus::Cancelled, 0, trInk("Anulowano przez użytkownika."));
    send_active_ = false;
    if (live_status_timer_)
        live_status_timer_->stop();
    if (live_action_btn_) {
        applyLiveActionUi(UiIcons::LiveAction::Start);
        live_action_btn_->setEnabled(true);
    }
    if (live_abort_btn_)
        live_abort_btn_->setVisible(false);
    if (live_progress_bar_)
        live_progress_bar_->setValue(0);
    updateLiveStatusBar(0, 0);
}

void MainWindow::onSendProgress(qint64 sent, qint64 total)
{
    send_progress_sent_ = sent;
    updateLiveStatusBar(sent, total);
}

void MainWindow::onSendLivePosition(double x, double y)
{
    double sc = last_job_settings_.protocol.plot_scale;
    if (!(sc > 0))
        sc = 1.0;
    const QPointF user(x / sc, y / sc);

    if (!live_scene_)
        return;

    if (!live_trail_item_) {
        live_trail_item_ = live_scene_->addPath(QPainterPath(), QPen(QColor(220, 40, 40), 0));
    }
    QPainterPath p = live_trail_item_->path();
    if (p.isEmpty())
        p.moveTo(user);
    else
        p.lineTo(user);
    live_trail_item_->setPath(p);

    if (plot_position_label_)
        plot_position_label_->setText(QStringLiteral("Pozycja (live): %1, %2")
                                          .arg(user.x(), 0, 'f', 2)
                                          .arg(user.y(), 0, 'f', 2));
}

void MainWindow::onSendFinished(bool ok, const QString& err)
{
    send_active_ = false;
    send_paused_ = false;
    if (live_status_timer_)
        live_status_timer_->stop();
    if (live_action_btn_) {
        applyLiveActionUi(UiIcons::LiveAction::Start);
        live_action_btn_->setEnabled(true);
    }
    if (live_abort_btn_)
        live_abort_btn_->setVisible(false);
    if (live_progress_bar_)
        live_progress_bar_->setValue(ok ? 100 : live_progress_bar_->value());
    updateLiveStatusBar(send_total_bytes_, send_total_bytes_);

    recordJobHistory(ok ? JobRunStatus::Complete : JobRunStatus::Error, 0, err);

    if (ok)
        QMessageBox::information(this, QStringLiteral("Inkcut"), trInk("Wysłano."));
    else if (!err.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Inkcut"), err);

    send_thread_.reset();
    send_worker_ = nullptr;
}

void MainWindow::applyStartupSettings(const AppSettings& settings)
{
    app_settings_ = settings;
    applyApplicationTheme(*qApp, settings.dock_style);
    applyAppSettingsToUi();
    retranslateUi();
}

DevicePlugin* MainWindow::activePlugin() const
{
    if (!plugin_loader_ || pipeline_cache_.plugin_id.isEmpty())
        return nullptr;
    return plugin_loader_->findById(pipeline_cache_.plugin_id);
}

void MainWindow::updateUnitSuffixes()
{
    const QString suf = unitSuffix(app_settings_.units);
    for (QDoubleSpinBox* s :
         {mat_w_spin_, mat_h_spin_, mat_pad_l_spin_, mat_pad_t_spin_, mat_pad_r_spin_,
          mat_pad_b_spin_, feed_after_spin_, copy_gap_x_spin_, copy_gap_y_spin_, copy_pad_l_spin_,
          copy_pad_t_spin_, copy_pad_r_spin_, copy_pad_b_spin_, plot_pad_l_spin_, plot_pad_t_spin_,
          plot_pad_r_spin_, plot_pad_b_spin_}) {
        if (s)
            s->setSuffix(suf);
    }
}

void MainWindow::retranslateUi()
{
    if (file_menu_)
        file_menu_->setTitle(trInk("Plik"));
    if (open_svg_action_)
        open_svg_action_->setText(trInk("Otwórz SVG / DXF…"));
    if (import_bitmap_action_)
        import_bitmap_action_->setText(trInk("Import bitmapy…"));
    if (device_menu_)
        device_menu_->setTitle(trInk("Urządzenie"));
    if (settings_menu_)
        settings_menu_->setTitle(trInk("Ustawienia"));
    if (help_menu_)
        help_menu_->setTitle(trInk("Pomoc"));

    if (left_tabs_ && left_tabs_->count() >= 4) {
        left_tabs_->setTabText(0, trInk("Materiał"));
        left_tabs_->setTabText(1, trInk("Grafika"));
        left_tabs_->setTabText(2, trInk("Warstwy"));
        left_tabs_->setTabText(3, trInk("Linie tnące"));
    }
    if (bottom_tabs_ && bottom_tabs_->count() >= 5) {
        bottom_tabs_->setTabText(0, trInk("Zadania"));
        bottom_tabs_->setTabText(1, trInk("Live"));
        bottom_tabs_->setTabText(2, trInk("Monitor"));
        bottom_tabs_->setTabText(3, trInk("Konsola"));
        bottom_tabs_->setTabText(4, trInk("Sterowanie"));
    }
}

void MainWindow::onConsoleCommand()
{
    if (!console_input_ || !console_output_)
        return;
    const QString cmd = console_input_->text().trimmed();
    console_input_->clear();
    if (cmd.isEmpty())
        return;
    console_output_->appendPlainText(QStringLiteral("> %1").arg(cmd));
    if (sendControlRawCommand(cmd))
        console_output_->appendPlainText(QStringLiteral("OK"));
    else
        console_output_->appendPlainText(trInk("(nie wysłano — brak połączenia lub błąd)"));
}

} // namespace inkcut
