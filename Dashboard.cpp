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
#include <QRandomGenerator>
#include <QFontDatabase>
#include <QScreen>

// --- Classe para o Widget de Sensor ---
class SensorWidget : public QFrame
{
    Q_OBJECT

public:
    SensorWidget(const QString &name, const QString &unit, const QString &color, QWidget *parent = nu>
    void setValue(double value);

private:
    QLabel *valueLabel;
    QProgressBar *progressBar;
};

// --- Classe Principal do Dashboard ---
class Dashboard : public QMainWindow
{
    Q_OBJECT

public:
    Dashboard(QWidget *parent = nullptr);
    ~Dashboard();

private slots:
    void updateData();

private:
    void setupUI();
    void applyStyles();

    QLabel *rpmCentralLabel;
    QProgressBar *rpmTopBar;
    QLabel *gearLabel;
    SensorWidget *mapSensor;
    SensorWidget *tpsSensor;
    SensorWidget *oilPressureSensor;
    SensorWidget *fuelPressureSensor;
    SensorWidget *batterySensor;
    SensorWidget *oilTempSensor;

    QTimer *timer;

    bool m_isSweepAnimationRunning = true;
    int m_sweepDirection = 1;
    double m_sweepRpm = 800;
};


// =================================================================
// Implementação do SensorWidget
// =================================================================
SensorWidget::SensorWidget(const QString &name, const QString &unit, const QString &color, QWidget *p>
    : QFrame(parent)
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
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(QString(
        "QProgressBar { background-color: #2c3e50; border: none; border-radius: 4px; height: 8px; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 4px; }"
    ).arg(color));

    layout->addLayout(headerLayout);
    layout->addWidget(valueLabel);
    layout->addWidget(progressBar);
}

void SensorWidget::setValue(double value)
{
    valueLabel->setText(QString::number(value, 'f', 1));
    progressBar->setValue(static_cast<int>(value));
}


// =================================================================
// Implementação do Dashboard
// =================================================================
Dashboard::Dashboard(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    applyStyles();

    setMinimumSize(800, 480); // previne colapso em telas pequenas

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Dashboard::updateData);
    timer->start(30);
}

Dashboard::~Dashboard() {}

void Dashboard::setupUI()
{
    setWindowTitle("Dashboard Pro V2");

    QWidget *centralWidget = new QWidget;
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QGridLayout *mainLayout = new QGridLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // --- BARRA DE RPM NO TOPO ---
    rpmTopBar = new QProgressBar();
    rpmTopBar->setRange(0, 11000);
    rpmTopBar->setValue(0);
    rpmTopBar->setTextVisible(false);
    rpmTopBar->setObjectName("rpmTopBar");
    mainLayout->addWidget(rpmTopBar, 0, 0, 1, 3);

    // --- PAINEL CENTRAL ---
    QFrame *centralDisplayFrame = new QFrame();
    centralDisplayFrame->setObjectName("centralDisplay");
    QVBoxLayout *centralDisplayLayout = new QVBoxLayout(centralDisplayFrame);
    centralDisplayLayout->setSpacing(0);
    centralDisplayLayout->addStretch();

    rpmCentralLabel = new QLabel("800");
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

    gearLabel = new QLabel("N");
    gearLabel->setObjectName("gearLabel");
    gearLabel->setAlignment(Qt::AlignCenter);
    gearLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    bottomLayout->addWidget(gearLabel);
    bottomLayout->addStretch();
    centralDisplayLayout->addLayout(bottomLayout);

    mainLayout->addWidget(centralDisplayFrame, 1, 1, 2, 1);
    // --- SENSORES ---
    mapSensor = new SensorWidget("MAP", "kPa", "#f39c12");
    tpsSensor = new SensorWidget("TPS", "%", "#2ecc71");
    batterySensor = new SensorWidget("BATERIA", "V", "#f1c40f");
    oilPressureSensor = new SensorWidget("ÓLEO P.", "bar", "#e74c3c");
    fuelPressureSensor = new SensorWidget("COMB P.", "bar", "#3498db");
    oilTempSensor = new SensorWidget("ÓLEO T.", "°C", "#9b59b6");

    QVBoxLayout *leftSensors = new QVBoxLayout();
    leftSensors->setSpacing(15);
    leftSensors->addWidget(mapSensor);
    leftSensors->addWidget(tpsSensor);
    leftSensors->addWidget(batterySensor);
    mainLayout->addLayout(leftSensors, 1, 0);

    QVBoxLayout *rightSensors = new QVBoxLayout();
    rightSensors->setSpacing(15);
    rightSensors->addWidget(oilPressureSensor);
    rightSensors->addWidget(fuelPressureSensor);
    rightSensors->addWidget(oilTempSensor);
    mainLayout->addLayout(rightSensors, 1, 2);

    // --- Botão Sair ---
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
            font-size: 150px; /* relativo à largura da tela */
            color: white;
        }
        QLabel#speedLabel {
            font-weight: bold;
            font-size: 80px;
            color: #3498db;
        }
        QLabel#speedUnitLabel {
            color: #7f8c8d;
            font-size: 50vw;
            font-weight: bold;
            padding-bottom: 20px;
        }
        QLabel#gearLabel {
            color: white;
            font-size: 70vw;
            font-weight: bold;
            border: 3px solid #3498db;
            border-radius: 15px;
            padding: 10px;
            min-width: 100px;
        }

        QLabel#sensorName { color: #ecf0f1; font-size: 16px; font-weight: bold; }
        QLabel#sensorUnit { color: #7f8c8d; font-size: 14px; }
        QLabel#sensorValue { color: white; font-size: 38px; font-weight: bold; }

        QPushButton#exitButton {
            background-color: #c0392b; color: white;
            font-weight: bold; font-size: 14px;
            border: none; border-radius: 5px;
            padding: 8px 16px;
        }
        QPushButton#exitButton:pressed { background-color: #e74c3c; }
    )");
}

void Dashboard::updateData()
{
    double rpm;

    if (m_isSweepAnimationRunning) {
        if (m_sweepDirection == 1) {
            m_sweepRpm += 300;
            if (m_sweepRpm >= 10500) { m_sweepRpm = 10500; m_sweepDirection = -1; }
        } else {
            m_sweepRpm -= 300;
            if (m_sweepRpm <= 800) { m_sweepRpm = 800; m_isSweepAnimationRunning = false; }
        }
        rpm = m_sweepRpm;

        speedLabel->setText("0");
        gearLabel->setText("N");
        mapSensor->setValue(0); tpsSensor->setValue(0);
        oilPressureSensor->setValue(0); fuelPressureSensor->setValue(0);
        batterySensor->setValue(0); oilTempSensor->setValue(0);

    } else {
        static double current_rpm = 800;
        static double speed = 0;
        static int gear = 0;

        if (current_rpm < 10500) current_rpm += QRandomGenerator::global()->bounded(350) + (100 * gea>
        else { current_rpm = 3000; if (gear < 6) gear++; }

        speed = (current_rpm / 110.0) * (gear * 0.6) + QRandomGenerator::global()->bounded(5);
        if(gear == 0) { speed = 0; current_rpm = 800 + QRandomGenerator::global()->bounded(100); }
        rpm = current_rpm;

        speedLabel->setText(QString::number(static_cast<int>(speed)));
        gearLabel->setText(gear == 0 ? "N" : QString::number(gear));
        mapSensor->setValue(80 + QRandomGenerator::global()->bounded(20.0));
        tpsSensor->setValue(rpm / 110.0 + QRandomGenerator::global()->bounded(5.0));
        oilPressureSensor->setValue(3.5 + (rpm / 5000.0) + QRandomGenerator::global()->generateDouble>
        fuelPressureSensor->setValue(3.0 + QRandomGenerator::global()->generateDouble() / 2.0);
        batterySensor->setValue(13.8 + QRandomGenerator::global()->generateDouble() / 2.0);
        oilTempSensor->setValue(90 + (rpm / 1000) + QRandomGenerator::global()->bounded(5.0));
    }

    rpmTopBar->setValue(static_cast<int>(rpm));
    rpmCentralLabel->setText(QString::number(static_cast<int>(rpm)));

    if (rpm > 9500) {
        rpmCentralLabel->setStyleSheet("color: #ff3838;");
        rpmTopBar->setStyleSheet("QProgressBar#rpmTopBar::chunk { background-color: #ff3838; }");
    } else if (rpm > 7000) {
        rpmCentralLabel->setStyleSheet("color: #f1c40f;");
        rpmTopBar->setStyleSheet("QProgressBar#rpmTopBar::chunk { background-color: #f1c40f; }");
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
