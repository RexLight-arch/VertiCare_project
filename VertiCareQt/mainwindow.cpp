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
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QDebug>
#include <QRadioButton>
#include <QStringList>
#include <QTabWidget>
#include <QtMath>
#include <QVBoxLayout>

static const char *APP_VERSION = "v1.5-polish";

class PlantSceneWidget : public QWidget
{
public:
    explicit PlantSceneWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(430, 360);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setState(bool irrigating, bool raining, bool vibrating, int servoAngle,
                  bool flame, bool tilt, bool airPoor)
    {
        m_irrigating = irrigating;
        m_raining = raining;
        m_vibrating = vibrating;
        m_servoAngle = servoAngle;
        m_flame = flame;
        m_tilt = tilt;
        m_airPoor = airPoor;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF canvas = rect().adjusted(10, 10, -10, -10);
        QLinearGradient background(canvas.topLeft(), canvas.bottomRight());
        background.setColorAt(0.0, QColor("#07131f"));
        background.setColorAt(0.46, QColor("#061922"));
        background.setColorAt(1.0, QColor("#02070b"));
        p.setPen(Qt::NoPen);
        p.setBrush(background);
        p.drawRoundedRect(canvas, 8, 8);

        QRadialGradient emeraldGlow(canvas.center() + QPointF(-80, -42), canvas.width() * 0.62);
        emeraldGlow.setColorAt(0.0, QColor(37, 255, 150, 68));
        emeraldGlow.setColorAt(0.42, QColor(20, 168, 130, 26));
        emeraldGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(emeraldGlow);
        p.drawRect(canvas);

        QRadialGradient cyanGlow(canvas.center() + QPointF(130, 70), canvas.width() * 0.52);
        cyanGlow.setColorAt(0.0, QColor(40, 206, 255, 48));
        cyanGlow.setColorAt(0.5, QColor(15, 74, 110, 18));
        cyanGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(cyanGlow);
        p.drawRect(canvas);

        p.save();
        p.setClipRect(canvas);
        p.setOpacity(0.42);
        p.setPen(QPen(QColor(75, 228, 255, 70), 1));
        for (int i = -6; i < 14; ++i) {
            const qreal y = canvas.top() + i * 34.0;
            p.drawLine(QPointF(canvas.left() - 60, y + 58),
                       QPointF(canvas.right() + 80, y - 28));
        }
        p.setPen(QPen(QColor(50, 255, 163, 50), 1));
        for (int i = -4; i < 12; ++i) {
            const qreal x = canvas.left() + i * 54.0;
            p.drawLine(QPointF(x, canvas.bottom() + 12),
                       QPointF(x + 220, canvas.top() - 34));
        }
        p.restore();

        p.save();
        p.setClipRect(canvas);
        for (int i = 0; i < 46; ++i) {
            const qreal x = canvas.left() + 28 + ((i * 73) % int(qMax(1.0, canvas.width() - 56)));
            const qreal y = canvas.top() + 26 + ((i * 47) % int(qMax(1.0, canvas.height() - 52)));
            const qreal radius = 1.2 + (i % 4) * 0.45;
            const QColor dot = (i % 3 == 0) ? QColor(93, 255, 177, 130) : QColor(76, 220, 255, 105);
            p.setPen(Qt::NoPen);
            p.setBrush(dot);
            p.drawEllipse(QPointF(x, y), radius, radius);
        }
        p.restore();

        const qreal wallW = qMin(canvas.width() * 0.58, 370.0);
        const qreal wallH = qMin(canvas.height() * 0.66, 278.0);
        const qreal depth = qMin(58.0, canvas.width() * 0.09);
        const qreal left = canvas.center().x() - wallW / 2.0 - depth * 0.15;
        const qreal top = canvas.center().y() - wallH / 2.0 - 8;

        QRadialGradient floorShadow(QPointF(left + wallW * 0.55, top + wallH + 42),
                                    wallW * 0.7, QPointF(left + wallW * 0.55, top + wallH + 42));
        floorShadow.setColorAt(0.0, QColor(0, 0, 0, 130));
        floorShadow.setColorAt(0.72, QColor(0, 185, 144, 38));
        floorShadow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(floorShadow);
        p.drawEllipse(QPointF(left + wallW * 0.55, top + wallH + 42), wallW * 0.52, 34);

        QPainterPath topFace;
        topFace.moveTo(left, top + 18);
        topFace.lineTo(left + depth, top - 16);
        topFace.lineTo(left + wallW + depth, top - 16);
        topFace.lineTo(left + wallW, top + 18);
        topFace.closeSubpath();
        QLinearGradient topGradient(left, top - 16, left + wallW, top + 18);
        topGradient.setColorAt(0, QColor(40, 180, 140, 105));
        topGradient.setColorAt(1, QColor(34, 242, 191, 35));
        p.setBrush(topGradient);
        p.setPen(QPen(QColor(88, 255, 208, 120), 1));
        p.drawPath(topFace);

        QPainterPath side;
        side.moveTo(left + wallW, top + 18);
        side.lineTo(left + wallW + depth, top - 16);
        side.lineTo(left + wallW + depth, top + wallH - 20);
        side.lineTo(left + wallW, top + wallH);
        side.closeSubpath();
        QLinearGradient sideGradient(left + wallW, top, left + wallW + depth, top + wallH);
        sideGradient.setColorAt(0, QColor(36, 137, 125, 92));
        sideGradient.setColorAt(1, QColor(7, 38, 47, 210));
        p.setBrush(sideGradient);
        p.setPen(QPen(QColor(66, 238, 255, 105), 1));
        p.drawPath(side);

        QPainterPath wall;
        wall.moveTo(left, top + 18);
        wall.lineTo(left + wallW, top + 18);
        wall.lineTo(left + wallW, top + wallH);
        wall.lineTo(left, top + wallH - 18);
        wall.closeSubpath();
        QLinearGradient wallGradient(left, top, left + wallW, top + wallH);
        wallGradient.setColorAt(0, QColor(29, 93, 77, 210));
        wallGradient.setColorAt(0.52, QColor(13, 50, 46, 220));
        wallGradient.setColorAt(1, QColor(7, 25, 34, 235));
        p.setBrush(wallGradient);
        p.setPen(QPen(QColor(85, 255, 190, 155), 1.5));
        p.drawPath(wall);
        p.setPen(QPen(QColor(62, 239, 255, 80), 7, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(wall);
        p.setPen(QPen(QColor(92, 255, 190, 170), 1.2));
        p.drawPath(wall);

        const int columns = 5;
        const int rows = 4;
        const qreal cellW = wallW / columns;
        const qreal cellH = (wallH - 18) / rows;
        p.setPen(QPen(QColor(86, 255, 210, 45), 1));
        for (int col = 1; col < columns; ++col) {
            const qreal x = left + cellW * col;
            p.drawLine(QPointF(x, top + 22), QPointF(x, top + wallH - 8));
        }
        for (int row = 1; row < rows; ++row) {
            const qreal y = top + 22 + cellH * row;
            p.drawLine(QPointF(left + 10, y - 3), QPointF(left + wallW - 8, y + 2));
        }

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < columns; ++col) {
                const qreal cx = left + cellW * (col + 0.5);
                const qreal cy = top + 30 + cellH * (row + 0.5);
                const QColor leaf = ((row + col) % 3 == 0)
                        ? QColor("#5ff28f") : ((row + col) % 3 == 1)
                        ? QColor("#23c879") : QColor("#a2ff95");

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 90));
                p.drawEllipse(QPointF(cx + 8, cy + 9), cellW * 0.3, cellH * 0.25);

                QRadialGradient leafGlow(QPointF(cx, cy), cellW * 0.5);
                leafGlow.setColorAt(0.0, leaf.lighter(145));
                leafGlow.setColorAt(0.58, leaf);
                leafGlow.setColorAt(1.0, leaf.darker(155));
                p.setBrush(leafGlow);
                p.drawEllipse(QPointF(cx - 10, cy), cellW * 0.25, cellH * 0.22);
                p.setBrush(leaf.lighter(118));
                p.drawEllipse(QPointF(cx + 13, cy - 5), cellW * 0.23, cellH * 0.2);
                p.setBrush(leaf.darker(108));
                p.drawEllipse(QPointF(cx + 2, cy + 12), cellW * 0.26, cellH * 0.2);

                p.setBrush(QColor(218, 255, 246, 170));
                p.drawEllipse(QPointF(cx + cellW * 0.11, cy - cellH * 0.12), 2.2, 1.7);
                p.setBrush(QColor(73, 255, 190, 80));
                p.drawEllipse(QPointF(cx - cellW * 0.08, cy + cellH * 0.08), cellW * 0.18, cellH * 0.08);
            }
        }

        p.setPen(QPen(QColor(95, 255, 193, 220), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(left + 18, top + 14), QPointF(left + wallW - 10, top + 14));
        p.setPen(QPen(QColor(96, 222, 255, 135), 2));
        for (int col = 0; col < columns; ++col) {
            const qreal x = left + cellW * (col + 0.5);
            p.drawLine(QPointF(x, top + 14), QPointF(x, top + 31));
        }

        if (m_irrigating) {
            p.setPen(QPen(QColor(90, 221, 255, 235), 3, Qt::SolidLine, Qt::RoundCap));
            for (int col = 0; col < columns; ++col) {
                const qreal x = left + cellW * (col + 0.5);
                for (int drop = 0; drop < 5; ++drop) {
                    const qreal y = top + 32 + drop * 18;
                    p.drawLine(QPointF(x, y), QPointF(x - 3, y + 8));
                }
            }
        }

        const QRectF base(left + wallW * 0.22, top + wallH + 4, wallW * 0.56, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#09130f"));
        p.drawRoundedRect(base.translated(5, 6), 6, 6);
        p.setBrush(QColor(20, 92, 76, 220));
        p.drawRoundedRect(base, 6, 6);
        p.setPen(QPen(QColor(92, 255, 204, 120), 1));
        p.drawRoundedRect(base, 6, 6);

        const QPointF servo(left + wallW + depth * 0.82, top + wallH * 0.7);
        p.setBrush(QColor(217, 246, 247, 225));
        p.setPen(QPen(QColor(87, 231, 255, 130), 1));
        p.drawRoundedRect(QRectF(servo.x() - 13, servo.y() - 18, 26, 36), 5, 5);
        p.setPen(QPen(m_irrigating ? QColor("#61d6ff") : QColor("#8aa39a"), 4,
                     Qt::SolidLine, Qt::RoundCap));
        const qreal radians = (m_servoAngle - 90) * 3.1415926 / 180.0;
        p.drawLine(servo, servo + QPointF(24 * qCos(radians), 24 * qSin(radians)));

        p.setPen(Qt::NoPen);
        p.setBrush(m_irrigating ? QColor("#3be58f") : QColor("#71877e"));
        p.drawEllipse(QPointF(canvas.left() + 34, canvas.bottom() - 34), 5, 5);
        p.setPen(QColor("#d7fff1"));
        p.setFont(QFont("Microsoft YaHei UI", 9));
        p.drawText(QRectF(canvas.left() + 48, canvas.bottom() - 47, 160, 26),
                   Qt::AlignVCenter, m_irrigating ? QStringLiteral("灌溉系统运行中")
                                                  : QStringLiteral("灌溉系统待机"));

        QStringList alerts;
        if (m_flame) alerts << QStringLiteral("火焰告警");
        if (m_tilt) alerts << QStringLiteral("发生倾斜");
        if (m_airPoor) alerts << QStringLiteral("空气异常");
        if (m_raining) alerts << QStringLiteral("检测到雨滴");
        if (m_vibrating) alerts << QStringLiteral("检测到振动");
        if (!alerts.isEmpty()) {
            p.setPen(QColor("#ffdf8a"));
            p.drawText(QRectF(canvas.right() - 230, canvas.bottom() - 47, 200, 26),
                       Qt::AlignRight | Qt::AlignVCenter, alerts.join(" / "));
        }
    }

private:
    bool m_irrigating = false;
    bool m_raining = false;
    bool m_vibrating = false;
    bool m_flame = false;
    bool m_tilt = false;
    bool m_airPoor = false;
    int m_servoAngle = 0;
};

static QString airQualityText(const QString &status, int percent)
{
    if (status == "poor" || percent >= 85)
        return QStringLiteral("立即通风");
    if (status == "warning" || percent >= 65)
        return QStringLiteral("建议通风");
    return QStringLiteral("清新");
}

static QString eventLevelText(const QString &level)
{
    if (level == "critical")
        return QStringLiteral("高危");
    if (level == "warning")
        return QStringLiteral("告警");
    if (level == "notice")
        return QStringLiteral("提醒");
    return QStringLiteral("信息");
}

static QColor eventLevelColor(const QString &level)
{
    if (level == "critical")
        return QColor("#ff806f");
    if (level == "warning")
        return QColor("#ffcc6b");
    if (level == "notice")
        return QColor("#66dff0");
    return QColor("#9dffd7");
}

static QString secondsText(int seconds)
{
    if (seconds <= 0)
        return QStringLiteral("0:00");
    return QString("%1:%2")
            .arg(seconds / 60)
            .arg(seconds % 60, 2, 10, QChar('0'));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();

    connect(&m_client, &OneNetClient::telemetryReceived,
            this, &MainWindow::applyTelemetry);
    connect(&m_client, &OneNetClient::bridgeStateChanged, this,
            [this](bool connected, const QString &message) {
        if (!m_lastTelemetryAt.isValid() || !connected)
            setConnectionState(connected, message);
    });
    connect(&m_client, &OneNetClient::logMessage, this,
            [](const QString &message) {
        qDebug().noquote() << "[VertiCare]" << message;
    });
    connect(&m_client, &OneNetClient::controlFinished,
            this, &MainWindow::handleControlFinished);
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::refreshTelemetry);
    connect(&m_healthTimer, &QTimer::timeout,
            this, &MainWindow::updateFreshness);

    if (!m_client.loadConfig(configPath())) {
        OneNetConfig defaults;
        defaults.productId = "i9912Q27NU";
        defaults.deviceName = "verticare_01";
        defaults.mockMode = true;
        m_client.setConfig(defaults);
    }

    setWindowTitle(QStringLiteral("VertiCare 垂直绿化管家 %1").arg(APP_VERSION));
    setMinimumSize(1080, 700);
    m_pollTimer.start(m_client.config().pollIntervalMs);
    m_healthTimer.start(1000);
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
    QLabel *subtitle = new QLabel(QStringLiteral("垂直绿化智能管家 · %1").arg(APP_VERSION), central);
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

    QTabWidget *tabs = new QTabWidget(central);
    tabs->setObjectName("mainTabs");

    QWidget *overviewPage = new QWidget(tabs);
    QGridLayout *overview = new QGridLayout(overviewPage);
    overview->setContentsMargins(0, 0, 0, 0);
    overview->setHorizontalSpacing(18);
    overview->setVerticalSpacing(18);
    overview->setColumnStretch(0, 3);
    overview->setColumnStretch(1, 2);

    m_plantScene = new PlantSceneWidget(overviewPage);
    overview->addWidget(m_plantScene, 0, 0, 2, 1);

    QWidget *overviewMetrics = new QWidget(overviewPage);
    QGridLayout *overviewGrid = new QGridLayout(overviewMetrics);
    overviewGrid->setContentsMargins(0, 0, 0, 0);
    overviewGrid->setSpacing(14);
    overviewGrid->addWidget(buildMetricCard("temperature", QStringLiteral("环境温度"), "°C", "#ffb86b"), 0, 0);
    overviewGrid->addWidget(buildMetricCard("airHumidity", QStringLiteral("空气湿度"), "%", "#66d7f2"), 0, 1);
    overviewGrid->addWidget(buildMetricCard("lightValue", QStringLiteral("环境亮度"), "%", "#ffe178"), 1, 0);
    overviewGrid->addWidget(buildMetricCard("rainDetected", QStringLiteral("雨滴状态"), "", "#72b7ff"), 1, 1);
    overviewGrid->addWidget(buildMetricCard("vibrationDetected", QStringLiteral("维护状态"), "", "#d3a5ff"), 2, 0);
    overviewGrid->addWidget(buildMetricCard("irrigationState", QStringLiteral("灌溉状态"), "", "#7ce3ac"), 2, 1);
    overview->addWidget(overviewMetrics, 0, 1, 2, 1);

    QWidget *safetyPage = new QWidget(tabs);
    QGridLayout *safetyGrid = new QGridLayout(safetyPage);
    safetyGrid->setContentsMargins(0, 0, 0, 0);
    safetyGrid->setSpacing(16);
    safetyGrid->addWidget(buildMetricCard("airQualityStatus", QStringLiteral("空气状态"), "", "#33b679"), 0, 0);
    safetyGrid->addWidget(buildMetricCard("airQualityPercent", QStringLiteral("通风指数"), "/100", "#6f9df5"), 0, 1);
    safetyGrid->addWidget(buildMetricCard("safetyAlert", QStringLiteral("安全状态"), "", "#ff8b6b"), 1, 0);
    safetyGrid->addWidget(buildMetricCard("flameDetected", QStringLiteral("火焰状态"), "", "#ff715c"), 1, 1);
    safetyGrid->addWidget(buildMetricCard("tiltDetected", QStringLiteral("倾斜状态"), "", "#a978f0"), 2, 0);
    safetyGrid->addWidget(buildMetricCard("sensorHealthy", QStringLiteral("传感器健康"), "", "#70c8ff"), 2, 1);

    QWidget *identityPage = new QWidget(tabs);
    QGridLayout *identityGrid = new QGridLayout(identityPage);
    identityGrid->setContentsMargins(0, 0, 0, 0);
    identityGrid->setSpacing(16);
    identityGrid->addWidget(buildMetricCard("rfidEnrollMode", QStringLiteral("录卡模式"), "", "#66d7f2"), 0, 0);
    identityGrid->addWidget(buildMetricCard("rfidAuthorized", QStringLiteral("认证状态"), "", "#7ce3ac"), 0, 1);
    identityGrid->addWidget(buildMetricCard("authorizedCardCount", QStringLiteral("已录入人数"), "人", "#ffe178"), 1, 0);
    identityGrid->addWidget(buildMetricCard("currentOperatorId", QStringLiteral("当前人员"), "", "#d3a5ff"), 1, 1);
    identityGrid->addWidget(buildMetricCard("authRemainingSec", QStringLiteral("授权剩余"), "", "#72b7ff"), 2, 0);
    identityGrid->addWidget(buildMetricCard("lastAccessEvent", QStringLiteral("最近刷卡"), "", "#ffb86b"), 2, 1);

    QWidget *controlPage = new QWidget(tabs);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPage);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(buildControlPanel());
    controlLayout->addStretch();

    tabs->addTab(overviewPage, QStringLiteral("总览"));
    tabs->addTab(safetyPage, QStringLiteral("安全"));
    tabs->addTab(identityPage, QStringLiteral("人员"));
    tabs->addTab(controlPage, QStringLiteral("控制"));
    tabs->addTab(buildEventPage(), QStringLiteral("事件"));
    root->addWidget(tabs, 1);

    setCentralWidget(central);
    setStyleSheet(
        "QWidget#appRoot { background: #061016; }"
        "QLabel { color: #d9fff1; font-family: 'Microsoft YaHei UI'; }"
        "QLabel#appTitle { color: #f2fff9; font-size: 42px; font-weight: 900; }"
        "QLabel#appSubtitle { color: #66dff0; font-size: 15px; font-weight: 600; }"
        "QLabel#connectionDot { background: #41ff9d; border-radius: 5px; }"
        "QLabel#connectionText { color: #b5fff0; font-size: 12px; font-weight: 700; }"
        "QLabel#updatedAt { color: #75aeb8; font-size: 12px; }"
        "QTabWidget::pane { border: none; top: -1px; }"
        "QTabBar::tab { background: rgba(7, 24, 34, 188); color: #78aeb9;"
        " border: 1px solid rgba(79, 211, 255, 74); border-radius: 8px; margin-right: 8px;"
        " padding: 10px 26px; min-width: 72px; font-size: 13px; font-weight: 800; }"
        "QTabBar::tab:selected { background: rgba(20, 214, 146, 54); color: #ecfff9;"
        " border-color: #32f3c1; }"
        "QTabBar::tab:hover { background: rgba(29, 88, 100, 188); color: #f2fff9; }"
        "QFrame#metricCard, QFrame#controlPanel {"
        " background: rgba(8, 25, 35, 206); border: 1px solid rgba(84, 235, 255, 92);"
        " border-radius: 8px; }"
        "QFrame#metricCard:hover { background: rgba(11, 39, 48, 224); border-color: #51ffd0; }"
        "QLabel[metricName='true'] { color: #a9e8ec; font-size: 17px; font-weight: 800; }"
        "QLabel[metricValue='true'] { color: #f1fff9; font-size: 36px; font-weight: 900; }"
        "QLabel[metricUnit='true'] { color: #66dff0; font-size: 13px; font-weight: 800; }"
        "QLabel#panelTitle { color: #f1fff9; font-size: 24px; font-weight: 900; }"
        "QLabel#modeBadge { background: rgba(45, 255, 162, 42); color: #9dffd7;"
        " border: 1px solid rgba(80, 255, 199, 130); border-radius: 8px; padding: 4px 10px;"
        " font-size: 11px; font-weight: 800; }"
        "QRadioButton, QCheckBox { color: #c5f7ee; spacing: 8px; font-size: 14px; font-weight: 600; }"
        "QRadioButton::indicator, QCheckBox::indicator { width: 16px; height: 16px; }"
        "QDoubleSpinBox { background: rgba(2, 13, 19, 220); color: #f1fff9;"
        " border: 1px solid rgba(92, 224, 255, 118); border-radius: 6px; padding: 7px 9px;"
        " min-width: 92px; selection-background-color: #1fdc91; }"
        "QPushButton { background: rgba(13, 36, 46, 230); color: #d8fff4;"
        " border: 1px solid rgba(92, 224, 255, 118); border-radius: 7px;"
        " padding: 9px 14px; font-weight: 800; }"
        "QPushButton:hover { background: rgba(19, 65, 76, 235); border-color: #5ee7ff; }"
        "QPushButton#primaryButton { background: #18d98a; color: #03100c; border: none; }"
        "QPushButton#primaryButton:hover { background: #38f4a6; }"
        "QPushButton#stopButton { background: rgba(255, 121, 92, 45); color: #ffd6ca;"
        " border-color: rgba(255, 150, 110, 150); }"
        "QPushButton:disabled { background: rgba(18, 31, 38, 180); color: #63818a;"
        " border-color: rgba(82, 115, 124, 90); }"
        "QLabel#commandStatus { color: #8dd9e2; font-size: 12px; font-weight: 700; }"
        "QListWidget#eventList { background: rgba(8, 25, 35, 206); color: #d9fff1;"
        " border: 1px solid rgba(84, 235, 255, 92); border-radius: 8px; padding: 8px;"
        " font-size: 15px; font-weight: 650; outline: none; }"
        "QListWidget#eventList::item { border-bottom: 1px solid rgba(93, 255, 202, 42);"
        " padding: 12px 10px; margin: 2px; }"
        "QListWidget#eventList::item:selected { background: rgba(45, 255, 162, 42);"
        " color: #ffffff; border-radius: 6px; }"
    );
}

QWidget *MainWindow::buildMetricCard(const QString &key, const QString &name,
                                     const QString &unit, const QString &accent)
{
    QFrame *card = new QFrame;
    card->setObjectName("metricCard");
    card->setMinimumHeight(96);
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
    value->setWordWrap(true);
    value->setMinimumWidth(0);
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
    m_applyButton = new QPushButton(QStringLiteral("应用设置"), panel);
    m_openButton = new QPushButton(QStringLiteral("立即灌溉"), panel);
    m_closeButton = new QPushButton(QStringLiteral("停止灌溉"), panel);
    m_applyButton->setObjectName("primaryButton");
    m_closeButton->setObjectName("stopButton");
    actions->addWidget(m_applyButton, 2);
    actions->addWidget(m_openButton);
    actions->addWidget(m_closeButton);
    layout->addLayout(actions);

    m_commandStatus = new QLabel(QStringLiteral("控制通道就绪"), panel);
    m_commandStatus->setObjectName("commandStatus");
    layout->addWidget(m_commandStatus);

    connect(m_applyButton, &QPushButton::clicked, this, &MainWindow::sendControl);
    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        m_manualMode->setChecked(true);
        m_manualIrrigation->setChecked(true);
        sendControl();
    });
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
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

QWidget *MainWindow::buildEventPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    QHBoxLayout *heading = new QHBoxLayout;
    QLabel *title = new QLabel(QStringLiteral("事件中心"), page);
    title->setObjectName("panelTitle");
    QLabel *badge = new QLabel(QStringLiteral("最近告警与提醒"), page);
    badge->setObjectName("modeBadge");
    heading->addWidget(title);
    heading->addStretch();
    heading->addWidget(badge);
    layout->addLayout(heading);

    QWidget *summary = new QWidget(page);
    QGridLayout *summaryGrid = new QGridLayout(summary);
    summaryGrid->setContentsMargins(0, 0, 0, 0);
    summaryGrid->setSpacing(14);
    summaryGrid->addWidget(buildMetricCard("systemSummary", QStringLiteral("系统概况"), "", "#7ce3ac"), 0, 0);
    summaryGrid->addWidget(buildMetricCard("latestEventSummary", QStringLiteral("最近事件"), "", "#ffb86b"), 0, 1);
    summaryGrid->addWidget(buildMetricCard("operatorSummary", QStringLiteral("维护人员"), "", "#d3a5ff"), 1, 0);
    summaryGrid->addWidget(buildMetricCard("irrigationSummary", QStringLiteral("灌溉策略"), "", "#72b7ff"), 1, 1);
    layout->addWidget(summary);

    m_eventList = new QListWidget(page);
    m_eventList->setObjectName("eventList");
    m_eventList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_eventList, 1);

    appendEvent(QStringLiteral("system"), QStringLiteral("info"),
                QStringLiteral("事件中心已就绪"));
    return page;
}

void MainWindow::refreshTelemetry()
{
    m_client.queryProperties();
}

void MainWindow::sendControl()
{
    setControlBusy(true);
    m_commandStatus->setText(QStringLiteral("正在发送控制指令..."));
    m_commandStatus->setStyleSheet("color: #ffd991;");
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
    const int airQualityPercent = telemetry.value("airQualityPercent").toInt();
    const QString airStatus = telemetry.value("airQualityStatus").toString();
    if (telemetry.contains("airQualityStatus") || telemetry.contains("airQualityPercent"))
        setMetric("airQualityStatus", airQualityText(airStatus, airQualityPercent));
    if (telemetry.contains("airQualityPercent"))
        setMetric("airQualityPercent", QString::number(
                      airQualityPercent));
    if (telemetry.contains("safetyAlert"))
        setMetric("safetyAlert", telemetry.value("safetyAlert").toBool()
                  ? QStringLiteral("告警") : QStringLiteral("正常"));
    if (telemetry.contains("flameDetected"))
        setMetric("flameDetected", telemetry.value("flameDetected").toBool()
                  ? QStringLiteral("火焰") : QStringLiteral("正常"));
    if (telemetry.contains("tiltDetected"))
        setMetric("tiltDetected", telemetry.value("tiltDetected").toBool()
                  ? QStringLiteral("倾斜") : QStringLiteral("正常"));
    if (telemetry.contains("mq135Healthy"))
        setMetric("mq135Healthy", telemetry.value("mq135Healthy").toBool()
                  ? QStringLiteral("正常") : QStringLiteral("异常"));
    if (telemetry.contains("sensorHealthy"))
        setMetric("sensorHealthy", telemetry.value("sensorHealthy").toBool()
                  ? QStringLiteral("正常") : QStringLiteral("异常"));
    if (telemetry.contains("vibrationDetected") || telemetry.contains("maintenanceEvent") ||
            telemetry.contains("tiltDetected")) {
        const bool tilted = telemetry.value("tiltDetected").toBool();
        const bool vibrating = telemetry.value("vibrationDetected").toBool() ||
                telemetry.value("maintenanceEvent").toBool();
        const bool authorized = telemetry.value("rfidAuthorized").toBool();
        setMetric("vibrationDetected", tilted ? QStringLiteral("倾斜")
                  : vibrating ? (authorized ? QStringLiteral("授权维护")
                                            : QStringLiteral("未授权维护"))
                              : QStringLiteral("待机"));
    }
    if (telemetry.contains("irrigationState"))
        setMetric("irrigationState", telemetry.value("irrigationState").toBool()
                  ? QStringLiteral("运行中") : QStringLiteral("已停止"));
    if (telemetry.contains("rfidEnrollMode"))
        setMetric("rfidEnrollMode", telemetry.value("rfidEnrollMode").toBool()
                  ? QStringLiteral("录入中") : QStringLiteral("关闭"));
    if (telemetry.contains("rfidAuthorized"))
        setMetric("rfidAuthorized", telemetry.value("rfidAuthorized").toBool()
                  ? QStringLiteral("已授权") : QStringLiteral("未授权"));
    if (telemetry.contains("authorizedCardCount"))
        setMetric("authorizedCardCount", QString::number(
                      telemetry.value("authorizedCardCount").toInt()));
    if (telemetry.contains("currentOperatorId"))
        setMetric("currentOperatorId", telemetry.value("currentOperatorId").toString());
    if (telemetry.contains("authRemainingSec"))
        setMetric("authRemainingSec", secondsText(
                      telemetry.value("authRemainingSec").toInt()));
    if (telemetry.contains("lastAccessEvent"))
        setMetric("lastAccessEvent", telemetry.value("lastAccessEvent").toString());

    const bool sensorHealthy = telemetry.value("sensorHealthy").toBool(true);
    const bool safetyAlert = telemetry.value("safetyAlert").toBool(false);
    const bool rfidAuthorized = telemetry.value("rfidAuthorized").toBool(false);
    const bool irrigationOn = telemetry.value("irrigationState").toBool(false);
    const bool manualMode = telemetry.value("controlMode").toInt() == 1;
    const QString currentOperator = telemetry.value("currentOperatorId").toString();
    setMetric("systemSummary", safetyAlert ? QStringLiteral("安全告警")
              : sensorHealthy ? QStringLiteral("运行正常")
                              : QStringLiteral("传感器异常"));
    setMetric("latestEventSummary", telemetry.value("lastEventMessage").toString(
                  QStringLiteral("暂无事件")));
    setMetric("operatorSummary", rfidAuthorized
              ? (currentOperator.isEmpty() ? QStringLiteral("已授权")
                                           : currentOperator)
              : QStringLiteral("未授权"));
    setMetric("irrigationSummary", QStringLiteral("%1 · %2")
              .arg(manualMode ? QStringLiteral("手动") : QStringLiteral("自动"),
                   irrigationOn ? QStringLiteral("灌溉中") : QStringLiteral("待机")));

    if (telemetry.contains("eventSequence")) {
        const int sequence = telemetry.value("eventSequence").toInt();
        if (sequence > 0 && sequence != m_lastEventSequence) {
            m_lastEventSequence = sequence;
            appendEvent(telemetry.value("lastEventType").toString(),
                        telemetry.value("lastEventLevel").toString(),
                        telemetry.value("lastEventMessage").toString());
        }
    }

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
    const bool vibrating = telemetry.value("vibrationDetected").toBool() ||
            telemetry.value("maintenanceEvent").toBool();
    const bool flame = telemetry.value("flameDetected").toBool();
    const bool tilted = telemetry.value("tiltDetected").toBool();
    const bool airPoor = telemetry.value("airQualityPercent").toInt() >= 85;
    const int angle = telemetry.value("servoAngle").toInt();
    m_plantScene->setState(irrigating, raining, vibrating, angle,
                           flame, tilted, airPoor);

    m_lastTelemetryAt = QDateTime::currentDateTime();
    m_sensorHealthy = telemetry.value("sensorHealthy").toBool(true);
    m_safetyAlert = telemetry.value("safetyAlert").toBool(false);
    m_updatedAt->setText(QStringLiteral("更新于 ") +
                         m_lastTelemetryAt.toString("HH:mm:ss"));
    if (m_safetyAlert) {
        setConnectionState(false, QStringLiteral("设备在线 · 安全告警"),
                           "#ff806f");
    } else if (!m_sensorHealthy) {
        setConnectionState(false, QStringLiteral("设备在线 · 关键传感器异常"),
                           "#ffb85c");
    } else {
        setConnectionState(true, m_client.config().mockMode
                           ? QStringLiteral("演示数据")
                           : QStringLiteral("设备在线 · 数据实时"));
    }
}

void MainWindow::setMetric(const QString &key, const QString &value)
{
    if (m_metrics.contains(key))
        m_metrics.value(key)->setText(value);
}

void MainWindow::updateFreshness()
{
    if (!m_lastTelemetryAt.isValid())
        return;

    const qint64 ageMs = m_lastTelemetryAt.msecsTo(QDateTime::currentDateTime());
    if (ageMs > m_client.config().dataStaleTimeoutMs) {
        setConnectionState(false,
                QStringLiteral("数据已 %1 秒未更新")
                .arg(qMax<qint64>(1, ageMs / 1000)), "#ffb85c");
    } else if (!m_sensorHealthy) {
        setConnectionState(false, QStringLiteral("设备在线 · 关键传感器异常"),
                           "#ffb85c");
    } else if (m_safetyAlert) {
        setConnectionState(false, QStringLiteral("设备在线 · 安全告警"),
                           "#ff806f");
    }
}

void MainWindow::handleControlFinished(bool ok, const QString &message)
{
    setControlBusy(false);
    m_commandStatus->setText(message);
    m_commandStatus->setStyleSheet(ok ? "color: #70e5a5;"
                                      : "color: #ff806f;");
}

void MainWindow::setControlBusy(bool busy)
{
    for (QPushButton *button : {m_applyButton, m_openButton, m_closeButton}) {
        if (button)
            button->setEnabled(!busy);
    }
}

void MainWindow::setConnectionState(bool online, const QString &message,
                                    const QString &color)
{
    const QString stateColor = color.isEmpty()
            ? (online ? "#45dc8b" : "#ff806f") : color;
    m_connectionDot->setStyleSheet(QString("background: %1; border-radius: 5px;")
                                   .arg(stateColor));
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

void MainWindow::appendEvent(const QString &type, const QString &level,
                             const QString &message)
{
    if (!m_eventList)
        return;

    const QString text = QStringLiteral("%1  %2  %3")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"),
                 eventLevelText(level),
                 message.isEmpty() ? type : message);
    QListWidgetItem *item = new QListWidgetItem(text);
    item->setForeground(eventLevelColor(level));
    m_eventList->insertItem(0, item);
    while (m_eventList->count() > 12) {
        delete m_eventList->takeItem(m_eventList->count() - 1);
    }
}

QString MainWindow::configPath() const
{
    return QDir(QApplication::applicationDirPath()).filePath("config.ini");
}
