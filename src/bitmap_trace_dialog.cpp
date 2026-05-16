// SPDX-License-Identifier: GPL-3.0-or-later

#include "bitmap_trace_dialog.hpp"

#include "i18n.hpp"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace inkcut {

namespace {

QString autoLabel()
{
    return trInk("Auto");
}

/// Spinbox z szerokością na N cyfr + miejsce na strzałki.
void sizeSpin(QAbstractSpinBox* spin, int digit_columns)
{
    const QFontMetrics fm(spin->font());
    const int content = fm.horizontalAdvance(QString(qMax(2, digit_columns), QLatin1Char('8')));
    const int frame = spin->style()->pixelMetric(QStyle::PM_SpinBoxFrameWidth) * 2;
    const int arrows = spin->style()->subControlRect(QStyle::CC_SpinBox, nullptr,
                                                     QStyle::SC_SpinBoxUp, spin).width();
    int w = content + frame + qMax(arrows, 18) + 16;
    spin->setFixedWidth(w);
    spin->setAlignment(Qt::AlignRight);
}

/// Combo dopasowany do najdłuższego itemu + arrow.
void sizeCombo(QComboBox* box)
{
    const QFontMetrics fm(box->font());
    int w = 0;
    for (int i = 0; i < box->count(); ++i)
        w = qMax(w, fm.horizontalAdvance(box->itemText(i)));
    const int arrow = 22;
    const int frame = 14;
    box->setFixedWidth(w + arrow + frame);
}

/// Etykieta o stałej szerokości + pole o stałej szerokości, reszta pusta.
QWidget* labeledRow(const QString& text, QWidget* field, QWidget* parent, int label_width)
{
    auto* row = new QWidget(parent);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    auto* lbl = new QLabel(text, row);
    lbl->setFixedWidth(label_width);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lay->addWidget(lbl, 0);
    lay->addWidget(field, 0, Qt::AlignVCenter | Qt::AlignLeft);
    lay->addStretch(1);
    return row;
}

QWidget* checkboxRow(QCheckBox* check, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(check, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lay->addStretch(1);
    return row;
}

class PreviewLabel final : public QLabel {
public:
    using QLabel::QLabel;
    std::function<void()> on_resize;
protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLabel::resizeEvent(e);
        if (on_resize)
            on_resize();
    }
};

} // namespace

bool runBitmapTraceDialog(QWidget* parent, const QImage& source, BitmapTraceOptions& options)
{
    if (source.isNull())
        return false;

    QDialog dlg(parent);
    dlg.setWindowTitle(trInk("Import bitmapy — parametry"));
    dlg.setMinimumSize(620, 600);
    dlg.resize(720, 720);

    auto* options_box = new QGroupBox(trInk("Parametry"), &dlg);
    auto* col1 = new QWidget(options_box);
    auto* col2 = new QWidget(options_box);

    auto* col1_lay = new QVBoxLayout(col1);
    col1_lay->setContentsMargins(0, 0, 0, 0);
    col1_lay->setSpacing(8);

    auto* col2_lay = new QVBoxLayout(col2);
    col2_lay->setContentsMargins(0, 0, 0, 0);
    col2_lay->setSpacing(8);

    auto* columns = new QHBoxLayout(options_box);
    columns->setContentsMargins(12, 14, 12, 12);
    columns->setSpacing(32);
    columns->addWidget(col1, 0, Qt::AlignTop);
    columns->addWidget(col2, 0, Qt::AlignTop);
    columns->addStretch(1);

    auto* highpass = new QComboBox(&dlg);
    highpass->addItem(autoLabel(), -1);
    highpass->addItem(trInk("Wyłączony"), 0);
    for (int r : {1, 2, 4, 8})
        highpass->addItem(QStringLiteral("%1 px").arg(r), r);

    auto* scale = new QComboBox(&dlg);
    scale->addItem(autoLabel(), -1);
    scale->addItem(QStringLiteral("1×"), 1);
    scale->addItem(QStringLiteral("2×"), 2);
    scale->addItem(QStringLiteral("3×"), 3);

    auto* thr_auto = new QCheckBox(trInk("Próg automatyczny (Otsu + 0.48)"), &dlg);
    thr_auto->setChecked(options.threshold < 0.0);

    auto* thr_val = new QDoubleSpinBox(&dlg);
    thr_val->setRange(0.0, 1.0);
    thr_val->setSingleStep(0.05);
    thr_val->setDecimals(2);
    thr_val->setValue(options.threshold >= 0.0 ? options.threshold : 0.48);
    thr_val->setEnabled(!thr_auto->isChecked());
    thr_val->setToolTip(trInk("Jak mkbitmap -t (np. 0.48). Niższy = więcej czerni."));

    auto* turd_auto = new QCheckBox(trInk("Automatyczny turdsize"), &dlg);
    turd_auto->setChecked(options.turdsize < 0);

    auto* turd_val = new QSpinBox(&dlg);
    turd_val->setRange(0, 500);
    turd_val->setValue(options.turdsize >= 0 ? options.turdsize : 2);
    turd_val->setEnabled(!turd_auto->isChecked());
    turd_val->setToolTip(trInk("Potrace -t: usuwa drobne plamki (w pikselach)."));

    auto* speck_auto = new QCheckBox(trInk("Automatyczne usuwanie szumu"), &dlg);
    speck_auto->setChecked(options.despeckle_min_area < 0);

    auto* speck_val = new QSpinBox(&dlg);
    speck_val->setRange(0, 500);
    speck_val->setValue(options.despeckle_min_area >= 0 ? options.despeckle_min_area : 8);
    speck_val->setEnabled(!speck_auto->isChecked());

    auto* invert = new QCheckBox(trInk("Odwróć jasność (ciemne tło)"), &dlg);
    invert->setChecked(options.force_invert);

    auto* gap_close = new QSpinBox(&dlg);
    gap_close->setRange(0, 16);
    gap_close->setValue(options.gap_close_radius);
    gap_close->setToolTip(
        trInk("Domyka cienkie białe szczeliny wewnątrz kształtu (np. linie na głowie logo). "
              "2–3 px zwykle wystarcza."));

    auto* fill_holes = new QSpinBox(&dlg);
    fill_holes->setRange(0, 10000);
    fill_holes->setSingleStep(50);
    fill_holes->setValue(options.fill_holes_max_area);
    fill_holes->setToolTip(
        trInk("Wypełnia zamknięte białe dziury o podanej powierzchni (w px). "
              "Użyj po „Zamknij szczeliny”, jeśli zostały małe oczka."));

    auto set_combo_by_data = [](QComboBox* box, int value) {
        for (int i = 0; i < box->count(); ++i) {
            if (box->itemData(i).toInt() == value) {
                box->setCurrentIndex(i);
                return;
            }
        }
    };
    set_combo_by_data(highpass, options.highpass_radius);
    set_combo_by_data(scale, options.scale_factor);

    sizeCombo(highpass);
    sizeCombo(scale);
    sizeSpin(thr_val, 4);
    sizeSpin(turd_val, 4);
    sizeSpin(speck_val, 4);
    sizeSpin(gap_close, 4);
    sizeSpin(fill_holes, 6);

    const QFontMetrics fm(dlg.font());
    const QStringList all_labels = {
        trInk("Filtr tła:"),       trInk("Skalowanie:"),        trInk("Próg:"),
        trInk("Turdsize:"),        trInk("Min. plamka:"),       trInk("Zamknij szczeliny:"),
        trInk("Wypełnij dziury:"),
    };
    int label_w = 0;
    for (const QString& s : all_labels)
        label_w = qMax(label_w, fm.horizontalAdvance(s));
    label_w += 8;

    col1_lay->addWidget(labeledRow(trInk("Filtr tła:"), highpass, col1, label_w));
    col1_lay->addWidget(labeledRow(trInk("Skalowanie:"), scale, col1, label_w));
    col1_lay->addWidget(checkboxRow(thr_auto, col1));
    col1_lay->addWidget(labeledRow(trInk("Próg:"), thr_val, col1, label_w));
    col1_lay->addWidget(checkboxRow(turd_auto, col1));
    col1_lay->addWidget(labeledRow(trInk("Turdsize:"), turd_val, col1, label_w));
    col1_lay->addStretch(1);

    col2_lay->addWidget(checkboxRow(speck_auto, col2));
    col2_lay->addWidget(labeledRow(trInk("Min. plamka:"), speck_val, col2, label_w));
    col2_lay->addWidget(labeledRow(trInk("Zamknij szczeliny:"), gap_close, col2, label_w));
    col2_lay->addWidget(labeledRow(trInk("Wypełnij dziury:"), fill_holes, col2, label_w));
    col2_lay->addWidget(checkboxRow(invert, col2));
    col2_lay->addStretch(1);

    auto* preview = new PreviewLabel(&dlg);
    preview->setMinimumSize(320, 300);
    preview->setAlignment(Qt::AlignCenter);
    preview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    preview->setText(trInk("Podgląd wektoryzacji"));

    auto* hint = new QLabel(
        trInk("Podgląd = ten sam pipeline co import. Cienkie białe linie wewnątrz sylwetki "
              "(np. logo): „Zamknij szczeliny” 2–3 px, ewentualnie „Wypełnij dziury” 100–500."),
        &dlg);
    hint->setWordWrap(true);

    auto read_options = [&]() -> BitmapTraceOptions {
        BitmapTraceOptions o;
        o.highpass_radius = highpass->currentData().toInt();
        o.scale_factor = scale->currentData().toInt();
        o.threshold = thr_auto->isChecked() ? -1.0 : thr_val->value();
        o.turdsize = turd_auto->isChecked() ? -1 : turd_val->value();
        o.despeckle_min_area = speck_auto->isChecked() ? -1 : speck_val->value();
        o.force_invert = invert->isChecked();
        o.gap_close_radius = gap_close->value();
        o.fill_holes_max_area = fill_holes->value();
        return o;
    };

    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(200);
    int preview_generation = 0;

    auto refresh_preview = [&]() {
        const int gen = ++preview_generation;
        preview->setPixmap({});
        preview->setText(trInk("Przeliczanie podglądu…"));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        const QSize box = preview->contentsRect().size().expandedTo(QSize(280, 200));
        const QPixmap pm = renderBitmapTracePreview(source, read_options(), box);

        if (gen != preview_generation)
            return;

        if (pm.isNull()) {
            preview->setPixmap({});
            preview->setText(trInk("Brak ścieżki — zmień parametry."));
            return;
        }
        preview->setPixmap(pm);
        preview->setText(QString());
    };

    const auto schedule_preview = [&]() { debounce.start(); };
    QObject::connect(&debounce, &QTimer::timeout, refresh_preview);

    const auto hook_combo = [&](QComboBox* box) {
        QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), schedule_preview);
        QObject::connect(box, &QComboBox::activated, schedule_preview);
    };
    hook_combo(highpass);
    hook_combo(scale);

    QObject::connect(thr_auto, &QCheckBox::toggled, [&](bool on) {
        thr_val->setEnabled(!on);
        schedule_preview();
    });
    QObject::connect(thr_val, QOverload<double>::of(&QDoubleSpinBox::valueChanged), schedule_preview);
    QObject::connect(thr_val, &QDoubleSpinBox::editingFinished, schedule_preview);

    QObject::connect(turd_auto, &QCheckBox::toggled, [&](bool on) {
        turd_val->setEnabled(!on);
        schedule_preview();
    });
    QObject::connect(turd_val, QOverload<int>::of(&QSpinBox::valueChanged), schedule_preview);
    QObject::connect(turd_val, &QSpinBox::editingFinished, schedule_preview);

    QObject::connect(speck_auto, &QCheckBox::toggled, [&](bool on) {
        speck_val->setEnabled(!on);
        schedule_preview();
    });
    QObject::connect(speck_val, QOverload<int>::of(&QSpinBox::valueChanged), schedule_preview);
    QObject::connect(speck_val, &QSpinBox::editingFinished, schedule_preview);

    QObject::connect(invert, &QCheckBox::toggled, schedule_preview);
    QObject::connect(gap_close, QOverload<int>::of(&QSpinBox::valueChanged), schedule_preview);
    QObject::connect(gap_close, &QSpinBox::editingFinished, schedule_preview);
    QObject::connect(fill_holes, QOverload<int>::of(&QSpinBox::valueChanged), schedule_preview);
    QObject::connect(fill_holes, &QSpinBox::editingFinished, schedule_preview);

    preview->on_resize = schedule_preview;

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(hint);
    layout->addWidget(options_box, 0);
    layout->addWidget(preview, 1);
    layout->addWidget(buttons);

    QTimer::singleShot(0, &dlg, refresh_preview);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    options = read_options();
    return true;
}

} // namespace inkcut
