#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFontDatabase>
#include <QScreen>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>

class SensorWidget : public QFrame
{
    Q_OBJECT

public:
    SensorWidget(const QString &name, const QString &unit, const QString &color,
                 double min = 0.0, double max = 100.0, QWidget *parent = nullptr);

    void setValue(double value);
    void setRange(double min, double max);

private:
    QLabel *valueLabel;
    QProgressBar *progressBar;
    double m_min;
    double m_max;
};

SensorWidget::SensorWidget(const QString &name, const QString &unit, const QString &color,
                           double min, double max, QWidget *parent)
    : QFrame(parent), m_min(min), m_max(max)
{
    this->setFrameShape(QFrame::StyledPanel);
    this->setObjectName("sensorFrame");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel* nameLabel = new QLabel(name.toUpper());
    nameLabel->setObjectName("sensorName");
    QLabel *unitLabel = new QLabel(unit);
    unitLabel->setObjectName("sensorUnit");

    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(unitLabel);

    valueLabel = new QLabel("0.0");
    valueLabel->setObjectName("sensorValue");
    valueLabel->setAlignment(Qt::AlignCenter);

    progressBar = new QProgressBar();
    progressBar->setRange(static_cast<int>(min*10), static_cast<int>(max*10));
    progressBar->setValue(static_cast<int>(min*10));
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(QString(
        "QProgressBar { background-color: #2c3e50; border: none; border-radius: 4px; height: 8px; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 4px; }"
    ).arg(color));

    layout->addLayout(headerLayout);
    layout->addWidget(valueLabel);
    layout->addWidget(progressBar);
}

void SensorWidget::setRange(double min, double max)
{
    m_min = min;
    m_max = max;
    progressBar->setRange(static_cast<int>(min*10), static_cast<int>(max*10));
}

void SensorWidget::setValue(double value)
{
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    valueLabel->setText(QString::number(value, 'f', 1));
    progressBar->setValue(static_cast<int>(value * 10));
}

// ---------------- Dashboard ----------------
class Dashboard : public QMainWindow
{
    Q_OBJECT

public:
    Dashboard(QWidget *parent = nullptr);
    ~Dashboard();

private slots:
    void updateData();
    void onTextMessageReceived(const QString &message);
    void onWsConnected();
    void onWsDisconnected();

private:
    void setupUI();
    void applyStyles();

    QLabel *rpmCentralLabel;
    QProgressBar *rpmTopBar;
    QLabel *speedLabel;

    QLabel *connLabel; // connection indicator

    SensorWidget *mapSensor;
    SensorWidget *tpsSensor;
    SensorWidget *batterySensor;
    SensorWidget *coolantSensor;

    QTimer *timer;

    QWebSocket *m_ws;
    QMutex m_jsonMutex;
    QJsonObject m_lastJson;
    bool m_haveData;
};

Dashboard::Dashboard(QWidget *parent)
    : QMainWindow(parent), m_ws(nullptr), m_haveData(false)
{
    setupUI();
    applyStyles();

    setMinimumSize(800, 480);

    m_ws = new QWebSocket();
    connect(m_ws, &QWebSocket::textMessageReceived, this, &Dashboard::onTextMessageReceived);
    connect(m_ws, &QWebSocket::connected, this, &Dashboard::onWsConnected);
    connect(m_ws, &QWebSocket::disconnected, this, &Dashboard::onWsDisconnected);

    m_ws->open(QUrl(QStringLiteral("ws://localhost:9090")));

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Dashboard::updateData);
    timer->start(33);
}

Dashboard::~Dashboard()
{
    if (m_ws) {
        m_ws->close();
        m_ws->deleteLater();
    }
}

void Dashboard::setupUI()
{
    setWindowTitle("Dashboard Automotivo");

    QWidget *centralWidget = new QWidget;
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QGridLayout *mainLayout = new QGridLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    rpmTopBar = new QProgressBar();
    rpmTopBar->setRange(0, 11000);
    rpmTopBar->setValue(0);
    rpmTopBar->setTextVisible(false);
    rpmTopBar->setObjectName("rpmTopBar");
    mainLayout->addWidget(rpmTopBar, 0, 0, 1, 3);

    // Connection indicator (top-right)
    connLabel = new QLabel("DESCONECTADO");
    connLabel->setObjectName("connLabel");
    connLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(connLabel, 0, 2, 1, 1, Qt::AlignRight | Qt::AlignVCenter);

    QFrame *centralDisplayFrame = new QFrame();
    centralDisplayFrame->setObjectName("centralDisplay");
    QVBoxLayout *centralDisplayLayout = new QVBoxLayout(centralDisplayFrame);
    centralDisplayLayout->setSpacing(0);
    centralDisplayLayout->addStretch();

    rpmCentralLabel = new QLabel("0");
    rpmCentralLabel->setObjectName("rpmCentralLabel");
    rpmCentralLabel->setAlignment(Qt::AlignCenter);
    rpmCentralLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    speedLabel = new QLabel("0");
    speedLabel->setObjectName("speedLabel");
    speedLabel->setAlignment(Qt::AlignCenter);
    speedLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QLabel* speedUnitLabel = new QLabel("km/h");
    speedUnitLabel->setObjectName("speedUnitLabel");
    speedUnitLabel->setAlignment(Qt::AlignCenter);

    centralDisplayLayout->addWidget(rpmCentralLabel);
    centralDisplayLayout->addWidget(speedLabel);
    centralDisplayLayout->addWidget(speedUnitLabel);
    centralDisplayLayout->addStretch();

    mainLayout->addWidget(centralDisplayFrame, 1, 1, 2, 1);

    mapSensor = new SensorWidget("MAP", "kPa", "#f39c12", 0.0, 250.0);
    tpsSensor = new SensorWidget("TPS", "%", "#2ecc71", 0.0, 100.0);
    batterySensor = new SensorWidget("BATERIA", "V", "#f1c40f", 10.0, 16.0);

    QVBoxLayout *leftSensors = new QVBoxLayout();
    leftSensors->setSpacing(15);
    leftSensors->addWidget(mapSensor);
    leftSensors->addWidget(tpsSensor);
    leftSensors->addWidget(batterySensor);
    mainLayout->addLayout(leftSensors, 1, 0);

    coolantSensor = new SensorWidget("COOLANT", "°C", "#9b59b6", -40.0, 215.0);
    QVBoxLayout *rightSensors = new QVBoxLayout();
    rightSensors->setSpacing(15);
    rightSensors->addWidget(coolantSensor);
    mainLayout->addLayout(rightSensors, 1, 2);

    QPushButton *exitButton = new QPushButton("SAIR");
    exitButton->setObjectName("exitButton");
    connect(exitButton, &QPushButton::clicked, &QApplication::quit);
    mainLayout->addWidget(exitButton, 3, 2, Qt::AlignRight | Qt::AlignBottom);

    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 3);
    mainLayout->setColumnStretch(2, 1);
    mainLayout->setRowStretch(1, 1);
}

void Dashboard::applyStyles()
{
    this->setStyleSheet(R"(
        QWidget#centralWidget { background-color: #1a1a1a; }
        QFrame#sensorFrame { background-color: #262626; border: 1px solid #333; border-radius: 8px; }
        QFrame#centralDisplay { border: none; }

        QProgressBar#rpmTopBar {
            border: 1px solid #333; border-radius: 8px;
            height: 40px;
            background-color: #262626;
        }
        QProgressBar#rpmTopBar::chunk {
            border-radius: 6px;
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2ecc71, stop:1 #27ae60);
        }

        QLabel#rpmCentralLabel {
            font-weight: bold;
            font-size: 120px;
            color: white;
        }
        QLabel#speedLabel {
            font-weight: bold;
            font-size: 80px;
            color: #3498db;
        }
        QLabel#speedUnitLabel {
            color: #7f8c8d;
            font-size: 24px;
            font-weight: bold;
            padding-bottom: 20px;
        }

        QLabel#sensorName { color: #ecf0f1; font-size: 16px; font-weight: bold; }
        QLabel#sensorUnit { color: #7f8c8d; font-size: 14px; }
        QLabel#sensorValue { color: white; font-size: 28px; font-weight: bold; }

        QLabel#connLabel { color: #e74c3c; font-weight: bold; font-size: 14px; }

        QPushButton#exitButton {
            background-color: #c0392b; color: white;
            font-weight: bold; font-size: 14px;
            border: none; border-radius: 5px;
            padding: 8px 16px;
        }
        QPushButton#exitButton:pressed { background-color: #e74c3c; }
    )");
}

void Dashboard::onWsConnected()
{
    qInfo("WebSocket connected to ws://localhost:9090");
    connLabel->setText("CONECTADO");
    connLabel->setStyleSheet("color: #2ecc71; font-weight: bold;");
}

void Dashboard::onWsDisconnected()
{
    qInfo("WebSocket disconnected");
    connLabel->setText("DESCONECTADO");
    connLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
}

void Dashboard::onTextMessageReceived(const QString &message)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QMutexLocker locker(&m_jsonMutex);
        m_lastJson = doc.object();
        m_haveData = true;
    }
}

void Dashboard::updateData()
{
    QJsonObject obj;
    {
        QMutexLocker locker(&m_jsonMutex);
        if (!m_haveData) {
            rpmTopBar->setValue(0);
            rpmCentralLabel->setText("0");
            speedLabel->setText("0");
            mapSensor->setValue(0);
            tpsSensor->setValue(0);
            batterySensor->setValue(0);
            coolantSensor->setValue(0);
            return;
        }
        obj = m_lastJson;
    }

    double rpm = 0.0;
    double speed = 0.0;
    double tps = 0.0;
    double map = 0.0;
    double battery = 0.0;
    double coolant = 0.0;

    if (obj.contains("rpm") && obj.value("rpm").isDouble()) rpm = obj.value("rpm").toDouble();
    if (obj.contains("speed") && obj.value("speed").isDouble()) speed = obj.value("speed").toDouble();
    if (obj.contains("tps") && obj.value("tps").isDouble()) tps = obj.value("tps").toDouble();
    if (obj.contains("map") && obj.value("map").isDouble()) map = obj.value("map").toDouble();
    if (obj.contains("battery") && obj.value("battery").isDouble()) battery = obj.value("battery").toDouble();
    if (obj.contains("coolant") && obj.value("coolant").isDouble()) coolant = obj.value("coolant").toDouble();

    rpmTopBar->setValue(static_cast<int>(rpm));
    rpmCentralLabel->setText(QString::number(static_cast<int>(rpm)));
    speedLabel->setText(QString::number(static_cast<int>(speed)));

    mapSensor->setValue(map);
    tpsSensor->setValue(tps);
    batterySensor->setValue(battery);
    coolantSensor->setValue(coolant);

    if (rpm > 6000) {
        rpmCentralLabel->setStyleSheet("color: #ff3838;");
        rpmTopBar->setStyleSheet("QProgressBar#rpmTopBar::chunk { background-color: #ff3838; }");
    } else if (rpm > 5500) {
        rpmCentralLabel->setStyleSheet("color: #f1c40f;");
        rpmTopBar->setStyleSheet("QProgressBar#rpmTopBar::chunk { background-color: #f1c40f; }");
    } else {
        rpmCentralLabel->setStyleSheet("color: white;");
        rpmTopBar->setStyleSheet("QProgressBar#rpmTopBar::chunk { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2ecc71, stop:1 #27ae60); }");
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Dashboard window;
    window.showFullScreen();
    return app.exec();
}

#include "main.moc"
