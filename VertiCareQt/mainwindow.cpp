#include "mainwindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QtMath>
#include <QVBoxLayout>

class PlantSceneWidget : public QWidget
{
public:
    explicit PlantSceneWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(430, 360);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setState(bool irrigating, bool raining, bool vibrating, int servoAngle)
    {
        m_irrigating = irrigating;
        m_raining = raining;
        m_vibrating = vibrating;
        m_servoAngle = servoAngle;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF canvas = rect().adjusted(18, 18, -18, -18);
        QLinearGradient background(canvas.topLeft(), canvas.bottomRight());
        background.setColorAt(0.0, QColor("#122920"));
        background.setColorAt(0.55, QColor("#0b1f19"));
        background.setColorAt(1.0, QColor("#071510"));
        p.setPen(Qt::NoPen);
        p.setBrush(background);
        p.drawRoundedRect(canvas, 18, 18);

        p.save();
        p.setOpacity(0.28);
        p.setPen(QPen(QColor("#63d69d"), 1));
        for (int i = 1; i < 7; ++i) {
            const qreal y = canvas.top() + canvas.height() * i / 7.0;
            p.drawLine(QPointF(canvas.left() + 24, y),
                       QPointF(canvas.right() - 24, y - 18));
        }
        p.restore();

        const qreal wallW = qMin(canvas.width() * 0.67, 420.0);
        const qreal wallH = qMin(canvas.height() * 0.72, 315.0);
        const qreal left = canvas.center().x() - wallW / 2.0 - 8;
        const qreal top = canvas.center().y() - wallH / 2.0 - 2;

        QPainterPath side;
        side.moveTo(left + wallW, top + 18);
        side.lineTo(left + wallW + 30, top);
        side.lineTo(left + wallW + 30, top + wallH - 18);
        side.lineTo(left + wallW, top + wallH);
        side.closeSubpath();
        p.setBrush(QColor("#183c2e"));
        p.drawPath(side);

        QPainterPath wall;
        wall.moveTo(left, top + 18);
        wall.lineTo(left + wallW, top + 18);
        wall.lineTo(left + wallW, top + wallH);
        wall.lineTo(left, top + wallH - 18);
        wall.closeSubpath();
        QLinearGradient wallGradient(left, top, left + wallW, top + wallH);
        wallGradient.setColorAt(0, QColor("#24543f"));
        wallGradient.setColorAt(1, QColor("#102b20"));
        p.setBrush(wallGradient);
        p.setPen(QPen(QColor("#3b745a"), 1));
        p.drawPath(wall);

        const int columns = 5;
        const int rows = 4;
        const qreal cellW = wallW / columns;
        const qreal cellH = (wallH - 18) / rows;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < columns; ++col) {
                const qreal cx = left + cellW * (col + 0.5);
                const qreal cy = top + 30 + cellH * (row + 0.5);
                const QColor leaf = ((row + col) % 3 == 0)
                        ? QColor("#52c878") : ((row + col) % 3 == 1)
                        ? QColor("#2f9d61") : QColor("#78d58a");

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 55));
                p.drawEllipse(QPointF(cx + 5, cy + 7), cellW * 0.28, cellH * 0.25);

                p.setBrush(leaf);
                p.drawEllipse(QPointF(cx - 10, cy), cellW * 0.25, cellH * 0.22);
                p.setBrush(leaf.lighter(118));
                p.drawEllipse(QPointF(cx + 13, cy - 5), cellW * 0.23, cellH * 0.2);
                p.setBrush(leaf.darker(108));
                p.drawEllipse(QPointF(cx + 2, cy + 12), cellW * 0.26, cellH * 0.2);
            }
        }

        p.setPen(QPen(QColor("#7ce9b0"), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(left + 18, top + 14), QPointF(left + wallW - 10, top + 14));
        p.setPen(QPen(QColor("#4da97a"), 2));
        for (int col = 0; col < columns; ++col) {
            const qreal x = left + cellW * (col + 0.5);
            p.drawLine(QPointF(x, top + 14), QPointF(x, top + 31));
        }

        if (m_irrigating) {
            p.setPen(QPen(QColor("#61d6ff"), 3, Qt::SolidLine, Qt::RoundCap));
            for (int col = 0; col < columns; ++col) {
                const qreal x = left + cellW * (col + 0.5);
                for (int drop = 0; drop < 3; ++drop) {
                    const qreal y = top + 34 + drop * 14;
                    p.drawLine(QPointF(x, y), QPointF(x - 2, y + 6));
                }
            }
        }

        const QRectF base(left + wallW * 0.22, top + wallH + 4, wallW * 0.56, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#09130f"));
        p.drawRoundedRect(base.translated(5, 6), 6, 6);
        p.setBrush(QColor("#294d3d"));
        p.drawRoundedRect(base, 6, 6);

        const QPointF servo(left + wallW + 28, top + wallH * 0.7);
        p.setBrush(QColor("#d7e4df"));
        p.drawRoundedRect(QRectF(servo.x() - 13, servo.y() - 18, 26, 36), 5, 5);
        p.setPen(QPen(m_irrigating ? QColor("#61d6ff") : QColor("#8aa39a"), 4,
                     Qt::SolidLine, Qt::RoundCap));
        const qreal radians = (m_servoAngle - 90) * 3.1415926 / 180.0;
        p.drawLine(servo, servo + QPointF(24 * qCos(radians), 24 * qSin(radians)));

        p.setPen(Qt::NoPen);
        p.setBrush(m_irrigating ? QColor("#3be58f") : QColor("#71877e"));
        p.drawEllipse(QPointF(canvas.left() + 34, canvas.bottom() - 34), 5, 5);
        p.setPen(QColor("#cfe8dc"));
        p.setFont(QFont("Microsoft YaHei UI", 9));
        p.drawText(QRectF(canvas.left() + 48, canvas.bottom() - 47, 160, 26),
                   Qt::AlignVCenter, m_irrigating ? QStringLiteral("灌溉系统运行中")
                                                  : QStringLiteral("灌溉系统待机"));

        QStringList alerts;
        if (m_raining) alerts << QStringLiteral("检测到雨滴");
        if (m_vibrating) alerts << QStringLiteral("检测到振动");
        if (!alerts.isEmpty()) {
            p.setPen(QColor("#ffd991"));
            p.drawText(QRectF(canvas.right() - 180, canvas.bottom() - 47, 150, 26),
                       Qt::AlignRight | Qt::AlignVCenter, alerts.join(" / "));
        }
    }

private:
    bool m_irrigating = false;
    bool m_raining = false;
    bool m_vibrating = false;
    int m_servoAngle = 0;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();

    connect(&m_client, &OneNetClient::telemetryReceived,
            this, &MainWindow::applyTelemetry);
    connect(&m_client, &OneNetClient::requestFinished, this,
            [this](bool ok, const QString &message) {
        setConnectionState(ok, ok ? QStringLiteral("OneNet 已连接")
                                  : QStringLiteral("连接异常: ") + message);
    });
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::refreshTelemetry);

    if (!m_client.loadConfig(configPath())) {
        OneNetConfig defaults;
        defaults.productId = "i9912Q27NU";
        defaults.deviceName = "verticare_01";
        defaults.mockMode = true;
        m_client.setConfig(defaults);
    }

    setWindowTitle(QStringLiteral("VertiCare 垂直绿化管家"));
    setMinimumSize(1080, 700);
    m_pollTimer.start(m_client.config().pollIntervalMs);
    refreshTelemetry();
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    central->setObjectName("appRoot");
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(28, 22, 28, 26);
    root->setSpacing(20);

    QHBoxLayout *header = new QHBoxLayout;
    QVBoxLayout *titles = new QVBoxLayout;
    titles->setSpacing(2);
    QLabel *title = new QLabel(QStringLiteral("VertiCare"), central);
    title->setObjectName("appTitle");
    QLabel *subtitle = new QLabel(QStringLiteral("垂直绿化智能管家"), central);
    subtitle->setObjectName("appSubtitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();

    m_connectionDot = new QLabel(central);
    m_connectionDot->setObjectName("connectionDot");
    m_connectionDot->setFixedSize(10, 10);
    m_connectionText = new QLabel(QStringLiteral("正在连接"), central);
    m_connectionText->setObjectName("connectionText");
    m_updatedAt = new QLabel("--:--:--", central);
    m_updatedAt->setObjectName("updatedAt");
    header->addWidget(m_connectionDot);
    header->addWidget(m_connectionText);
    header->addSpacing(16);
    header->addWidget(m_updatedAt);
    root->addLayout(header);

    QGridLayout *body = new QGridLayout;
    body->setHorizontalSpacing(18);
    body->setVerticalSpacing(18);
    body->setColumnStretch(0, 3);
    body->setColumnStretch(1, 2);

    m_plantScene = new PlantSceneWidget(central);
    body->addWidget(m_plantScene, 0, 0, 2, 1);

    QWidget *metrics = new QWidget(central);
    QGridLayout *metricGrid = new QGridLayout(metrics);
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setSpacing(14);
    metricGrid->addWidget(buildMetricCard("temperature", QStringLiteral("环境温度"), "°C", "#ffb86b"), 0, 0);
    metricGrid->addWidget(buildMetricCard("airHumidity", QStringLiteral("空气湿度"), "%", "#66d7f2"), 0, 1);
    metricGrid->addWidget(buildMetricCard("lightValue", QStringLiteral("环境亮度"), "%", "#ffe178"), 1, 0);
    metricGrid->addWidget(buildMetricCard("rainDetected", QStringLiteral("雨滴状态"), "", "#72b7ff"), 1, 1);
    metricGrid->addWidget(buildMetricCard("vibrationDetected", QStringLiteral("结构状态"), "", "#d3a5ff"), 2, 0);
    metricGrid->addWidget(buildMetricCard("irrigationState", QStringLiteral("灌溉状态"), "", "#7ce3ac"), 2, 1);
    body->addWidget(metrics, 0, 1);
    body->addWidget(buildControlPanel(), 1, 1);
    root->addLayout(body, 1);

    setCentralWidget(central);
    setStyleSheet(
        "QWidget#appRoot { background: #07120e; }"
        "QLabel { color: #d8e8e0; font-family: 'Microsoft YaHei UI'; }"
        "QLabel#appTitle { color: #f4fff9; font-size: 34px; font-weight: 700; }"
        "QLabel#appSubtitle { color: #7f998d; font-size: 14px; }"
        "QLabel#connectionDot { background: #45dc8b; border-radius: 5px; }"
        "QLabel#connectionText { color: #b7cdc2; font-size: 12px; }"
        "QLabel#updatedAt { color: #61776d; font-size: 12px; }"
        "QFrame#metricCard, QFrame#controlPanel {"
        " background: #10221b; border: 1px solid #234435; border-radius: 8px; }"
        "QFrame#metricCard:hover { background: #132920; border-color: #376b52; }"
        "QLabel[metricName='true'] { color: #9bb2a7; font-size: 15px; font-weight: 600; }"
        "QLabel[metricValue='true'] { color: #f5fff9; font-size: 31px; font-weight: 700; }"
        "QLabel[metricUnit='true'] { color: #779084; font-size: 13px; font-weight: 600; }"
        "QLabel#panelTitle { color: #effaf4; font-size: 18px; font-weight: 600; }"
        "QLabel#modeBadge { background: #173b2c; color: #70e5a5;"
        " border: 1px solid #296344; border-radius: 8px; padding: 3px 8px; font-size: 10px; }"
        "QRadioButton, QCheckBox { color: #b9cdc3; spacing: 8px; font-size: 12px; }"
        "QRadioButton::indicator, QCheckBox::indicator { width: 16px; height: 16px; }"
        "QDoubleSpinBox { background: #0a1813; color: #e8f6ef; border: 1px solid #29483a;"
        " border-radius: 6px; padding: 7px 9px; min-width: 92px; }"
        "QPushButton { background: #163429; color: #cce3d7; border: 1px solid #285440;"
        " border-radius: 7px; padding: 9px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #1d4636; border-color: #4c9f75; }"
        "QPushButton#primaryButton { background: #38c979; color: #062015; border: none; }"
        "QPushButton#primaryButton:hover { background: #51df91; }"
        "QPushButton#stopButton { background: #2b2119; color: #ffc58e; border-color: #61452e; }"
    );
}

QWidget *MainWindow::buildMetricCard(const QString &key, const QString &name,
                                     const QString &unit, const QString &accent)
{
    QFrame *card = new QFrame;
    card->setObjectName("metricCard");
    card->setMinimumHeight(112);
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(17, 14, 17, 14);
    layout->setSpacing(8);

    QHBoxLayout *caption = new QHBoxLayout;
    QLabel *accentBar = new QLabel(card);
    accentBar->setFixedSize(4, 16);
    accentBar->setStyleSheet(QString("background: %1; border-radius: 1px;").arg(accent));
    QLabel *nameLabel = new QLabel(name, card);
    nameLabel->setProperty("metricName", true);
    caption->addWidget(accentBar);
    caption->addWidget(nameLabel);
    caption->addStretch();
    layout->addLayout(caption);

    QHBoxLayout *reading = new QHBoxLayout;
    QLabel *value = new QLabel("--", card);
    value->setProperty("metricValue", true);
    QLabel *unitLabel = new QLabel(unit, card);
    unitLabel->setProperty("metricUnit", true);
    reading->addWidget(value);
    reading->addWidget(unitLabel, 0, Qt::AlignBottom);
    reading->addStretch();
    layout->addLayout(reading);
    m_metrics.insert(key, value);
    return card;
}

QWidget *MainWindow::buildControlPanel()
{
    QFrame *panel = new QFrame;
    panel->setObjectName("controlPanel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 15, 18, 16);
    layout->setSpacing(12);

    QHBoxLayout *heading = new QHBoxLayout;
    QLabel *title = new QLabel(QStringLiteral("灌溉控制"), panel);
    title->setObjectName("panelTitle");
    m_modeBadge = new QLabel(QStringLiteral("自动模式"), panel);
    m_modeBadge->setObjectName("modeBadge");
    heading->addWidget(title);
    heading->addStretch();
    heading->addWidget(m_modeBadge);
    layout->addLayout(heading);

    QHBoxLayout *modes = new QHBoxLayout;
    m_autoMode = new QRadioButton(QStringLiteral("自动"), panel);
    m_manualMode = new QRadioButton(QStringLiteral("手动"), panel);
    m_autoMode->setChecked(true);
    QButtonGroup *modeGroup = new QButtonGroup(panel);
    modeGroup->addButton(m_autoMode);
    modeGroup->addButton(m_manualMode);
    modes->addWidget(m_autoMode);
    modes->addWidget(m_manualMode);
    modes->addStretch();
    layout->addLayout(modes);

    QHBoxLayout *thresholds = new QHBoxLayout;
    m_openThreshold = new QDoubleSpinBox(panel);
    m_closeThreshold = new QDoubleSpinBox(panel);
    for (QDoubleSpinBox *spin : {m_openThreshold, m_closeThreshold}) {
        spin->setRange(0.0, 100.0);
        spin->setDecimals(1);
        spin->setSingleStep(0.5);
        spin->setSuffix("%");
    }
    m_openThreshold->setValue(45.0);
    m_closeThreshold->setValue(60.0);
    thresholds->addWidget(new QLabel(QStringLiteral("开启阈值"), panel));
    thresholds->addWidget(m_openThreshold);
    thresholds->addSpacing(8);
    thresholds->addWidget(new QLabel(QStringLiteral("关闭阈值"), panel));
    thresholds->addWidget(m_closeThreshold);
    thresholds->addStretch();
    layout->addLayout(thresholds);

    m_manualIrrigation = new QCheckBox(QStringLiteral("手动灌溉指令"), panel);
    layout->addWidget(m_manualIrrigation);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *applyButton = new QPushButton(QStringLiteral("应用设置"), panel);
    QPushButton *openButton = new QPushButton(QStringLiteral("立即灌溉"), panel);
    QPushButton *closeButton = new QPushButton(QStringLiteral("停止灌溉"), panel);
    applyButton->setObjectName("primaryButton");
    closeButton->setObjectName("stopButton");
    actions->addWidget(applyButton, 2);
    actions->addWidget(openButton);
    actions->addWidget(closeButton);
    layout->addLayout(actions);

    connect(applyButton, &QPushButton::clicked, this, &MainWindow::sendControl);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        m_manualMode->setChecked(true);
        m_manualIrrigation->setChecked(true);
        sendControl();
    });
    connect(closeButton, &QPushButton::clicked, this, [this]() {
        m_manualMode->setChecked(true);
        m_manualIrrigation->setChecked(false);
        sendControl();
    });
    connect(m_autoMode, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_modeBadge->setText(QStringLiteral("自动模式"));
        m_manualIrrigation->setEnabled(!checked);
    });
    connect(m_manualMode, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_modeBadge->setText(QStringLiteral("手动模式"));
    });

    return panel;
}

void MainWindow::refreshTelemetry()
{
    m_client.queryProperties();
}

void MainWindow::sendControl()
{
    setConnectionState(true, QStringLiteral("正在发送控制指令"));
    m_client.setProperties(controlParamsFromUi());
}

void MainWindow::applyTelemetry(const QJsonObject &telemetry)
{
    if (telemetry.contains("temperature"))
        setMetric("temperature", QString::number(telemetry.value("temperature").toDouble(), 'f', 1));
    if (telemetry.contains("airHumidity"))
        setMetric("airHumidity", QString::number(telemetry.value("airHumidity").toDouble(), 'f', 1));
    if (telemetry.contains("lightValue")) {
        const int raw = telemetry.value("lightValue").toInt();
        const int brightRaw = 1200;
        const int darkRaw = 3200;
        const int brightness = qBound(
                    0, qRound((darkRaw - raw) * 100.0 / (darkRaw - brightRaw)), 100);
        setMetric("lightValue", QString::number(brightness));
    }
    if (telemetry.contains("rainDetected"))
        setMetric("rainDetected", telemetry.value("rainDetected").toBool()
                  ? QStringLiteral("有雨") : QStringLiteral("干燥"));
    if (telemetry.contains("vibrationDetected"))
        setMetric("vibrationDetected", telemetry.value("vibrationDetected").toBool()
                  ? QStringLiteral("振动") : QStringLiteral("平稳"));
    if (telemetry.contains("irrigationState"))
        setMetric("irrigationState", telemetry.value("irrigationState").toBool()
                  ? QStringLiteral("运行中") : QStringLiteral("已停止"));

    if (telemetry.contains("controlMode")) {
        const bool manual = telemetry.value("controlMode").toInt() == 1;
        m_manualMode->setChecked(manual);
        m_autoMode->setChecked(!manual);
    }
    if (telemetry.contains("manualIrrigation"))
        m_manualIrrigation->setChecked(telemetry.value("manualIrrigation").toBool());
    if (telemetry.contains("openThreshold"))
        m_openThreshold->setValue(telemetry.value("openThreshold").toDouble());
    if (telemetry.contains("closeThreshold"))
        m_closeThreshold->setValue(telemetry.value("closeThreshold").toDouble());

    const bool irrigating = telemetry.value("irrigationState").toBool();
    const bool raining = telemetry.value("rainDetected").toBool();
    const bool vibrating = telemetry.value("vibrationDetected").toBool();
    const int angle = telemetry.value("servoAngle").toInt();
    m_plantScene->setState(irrigating, raining, vibrating, angle);

    m_updatedAt->setText(QStringLiteral("更新于 ") +
                         QDateTime::currentDateTime().toString("HH:mm:ss"));
    setConnectionState(true, m_client.config().mockMode
                       ? QStringLiteral("演示数据")
                       : QStringLiteral("OneNet 已连接"));
}

void MainWindow::setMetric(const QString &key, const QString &value)
{
    if (m_metrics.contains(key))
        m_metrics.value(key)->setText(value);
}

void MainWindow::setConnectionState(bool online, const QString &message)
{
    m_connectionDot->setStyleSheet(QString("background: %1; border-radius: 5px;")
                                   .arg(online ? "#45dc8b" : "#ff806f"));
    m_connectionText->setText(message);
}

QJsonObject MainWindow::controlParamsFromUi() const
{
    QJsonObject params;
    params.insert("controlMode", m_manualMode->isChecked() ? 1 : 0);
    params.insert("manualIrrigation", m_manualIrrigation->isChecked());
    params.insert("openThreshold", m_openThreshold->value());
    params.insert("closeThreshold", m_closeThreshold->value());
    return params;
}

QString MainWindow::configPath() const
{
    return QDir(QApplication::applicationDirPath()).filePath("config.ini");
}
