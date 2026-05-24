#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTime>
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_timer(new QTimer(this))
    , m_isRunning(false)
    , m_tickCounter(0)
{
    ui->setupUi(this);

    // Настройка лога
    ui->logTextEdit->setReadOnly(true);
    ui->logTextEdit->setStyleSheet(
        "background-color: #1e1e1e; "
        "color: #d4d4d4; "
        "font-family: 'Consolas', monospace; "
        "font-size: 10pt;"
        );

    // Создаём модель
    m_model = new AquariumModel(this);

    // Подключаем сигналы модели к обновлению UI
    connect(m_model, &AquariumModel::temperatureChanged, this, [this](double v) {
        ui->tempLabel->setText(QString("%1 C").arg(v, 0, 'f', 1));
    });
    connect(m_model, &AquariumModel::waterLevelChanged, this, [this](int v) {
        ui->waterLabel->setText(QString("%1 %").arg(v));
        addLogMessage("Уровень воды изменился: " + QString::number(v) + "%");
    });
    connect(m_model, &AquariumModel::foodAmountChanged, this, [this](int v) {
        ui->foodLabel->setText(QString("%1 %").arg(v));
        if (v < 15) {
            addLogMessage("ВНИМАНИЕ: Осталось мало корма (" + QString::number(v) + "%)");
        }
    });
    connect(m_model, &AquariumModel::lightBrightnessChanged, this, [this](int v) {
        ui->lightLabel->setText(v <= 20
            ? QString("выключен")
            : QString("%1 %").arg(v));
    });
    connect(m_model, &AquariumModel::filterFlowChanged, this, [this](int v) {
        ui->filterLabel->setText(QString("%1 %").arg(v));
        addLogMessage("Скорость потока фильтра: " + QString::number(v) + "%");
    });
    connect(m_model, &AquariumModel::alarmTriggered, this, &MainWindow::onAlarm);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onSimulationTimer);

    // Инициализация UI начальными значениями
    updateUIFromModel();

    addLogMessage("Система умного аквариума запущена");

    // Создаём сцену
    aquariumScene = new QGraphicsScene(this);
    aquariumScene->setSceneRect(0, 0, 686, 500);
    ui->graphicsView->setScene(aquariumScene);

    // Дно — слой крупного песка
    aquariumScene->addRect(0, 420, 686, 80, Qt::NoPen, QBrush(QColor(210, 180, 130)));
    // Поверх — мелкий светлый песок
    aquariumScene->addRect(0, 430, 686, 20, Qt::NoPen, QBrush(QColor(235, 210, 160)));

    // Камни на дне
    auto addStone = [&](int x, int y, int w, int h, QColor c) {
        auto* stone = aquariumScene->addEllipse(x, y, w, h, Qt::NoPen, QBrush(c));
        stone->setZValue(2);
    };
    addStone(60,  415, 55, 28, QColor(150, 140, 130));
    addStone(80,  420, 35, 18, QColor(180, 170, 155));
    addStone(200, 418, 45, 24, QColor(140, 130, 120));
    addStone(220, 423, 28, 15, QColor(170, 160, 148));
    addStone(400, 416, 60, 26, QColor(155, 145, 133));
    addStone(550, 419, 40, 20, QColor(148, 138, 126));
    addStone(570, 424, 25, 14, QColor(175, 165, 152));

    // Водоросли — ломаные линии
    auto addSeaweed = [&](int baseX, QColor c) {
        QPen pen(c, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        // стебель из трёх отрезков
        aquariumScene->addLine(baseX,     430, baseX-8,  390, pen)->setZValue(3);
        aquariumScene->addLine(baseX-8,   390, baseX+6,  355, pen)->setZValue(3);
        aquariumScene->addLine(baseX+6,   355, baseX-4,  320, pen)->setZValue(3);
        // маленький листик на вершине
        QPen leafPen(c.lighter(120), 3, Qt::SolidLine, Qt::RoundCap);
        aquariumScene->addLine(baseX-4, 320, baseX+14, 308, leafPen)->setZValue(3);
        aquariumScene->addLine(baseX-4, 320, baseX-16, 310, leafPen)->setZValue(3);
    };
    addSeaweed(130, QColor(60, 160, 80));
    addSeaweed(310, QColor(40, 140, 100));
    addSeaweed(480, QColor(70, 155, 70));
    addSeaweed(610, QColor(50, 145, 90));

    // Вода — полупрозрачный слой поверх деталей
    waterItem = aquariumScene->addRect(0, 0, 686, 420, Qt::NoPen, QBrush(QColor(100, 180, 255, 60)));
    waterItem->setZValue(1);

    // Затемнение для освещения
    darkOverlay = aquariumScene->addRect(0, 0, 686, 500, Qt::NoPen, QBrush(QColor(0, 0, 0, 0)));
    darkOverlay->setZValue(10);

    // Кормушка — закреплена сверху справа
    feeder = aquariumScene->addRect(10, 0, 65, 55, QPen(QColor(130, 90, 40), 2), QBrush(QColor(190, 145, 80)));
    feeder->setZValue(5);
    // Крышка кормушки
    aquariumScene->addRect(0, 0, 85, 10, QPen(QColor(110, 75, 30), 1), QBrush(QColor(160, 120, 60)))->setZValue(5);
    // Индикатор корма
    feederLevel = aquariumScene->addRect(15, 15, 55, 35, Qt::NoPen, QBrush(QColor(255, 210, 60)));
    feederLevel->setZValue(6);

    create_fishes(3);
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MainWindow::moveFishes);
    animationTimer->start(50);  // 20 кадров в секунду

    connect(m_model, &AquariumModel::waterLevelChanged, this, &MainWindow::updateWaterLevel);
    connect(m_model, &AquariumModel::lightBrightnessChanged, this, &MainWindow::updateLight);

    fishesInSafeZone = 0;
    evacuationModeActive = false;
    updateWaterLevel(85);  // начальный уровень
    evacuationCheckTimer = new QTimer(this);
    connect(evacuationCheckTimer, &QTimer::timeout, this, &MainWindow::checkAllFishInSafeZone);
    evacuationCheckTimer->setInterval(500);

    lightTransitionTimer = new QTimer(this);
    connect(lightTransitionTimer, &QTimer::timeout, this, &MainWindow::updateLightTransition);
    lightTransitionTimer->setInterval(100);  // каждые 100 мс

    targetBrightness = 0;
    lightTransitionActive = false;
    lightStep = 0;

    connect(m_model, &AquariumModel::foodAmountChanged, this, &MainWindow::updateFeederDisplay);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateUIFromModel()
{
    ui->tempLabel->setText(QString("%1 C").arg(m_model->getTemperature(), 0, 'f', 1));
    ui->waterLabel->setText(QString("%1 %").arg(m_model->getWaterLevel()));
    ui->foodLabel->setText(QString("%1 %").arg(m_model->getFoodAmount()));
    int lb = m_model->getLightBrightness();
    ui->lightLabel->setText(lb <= 20
        ? QString("выключен")
        : QString("%1%").arg(lb));
    ui->filterLabel->setText(QString("%1 %").arg(m_model->getFilterFlow()));
}

void MainWindow::addLogMessage(const QString& message)
{
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    ui->logTextEdit->append(timestamp + " - " + message);

    // Автопрокрутка вниз
    QTextCursor cursor = ui->logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->logTextEdit->setTextCursor(cursor);
}

void MainWindow::onAlarm(QString message)
{
    addLogMessage("ТРЕВОГА: " + message);

    QMessageBox box(this);
    box.setWindowTitle("Тревога!");
    box.setText(message);
    box.setIcon(QMessageBox::Warning);
    box.setStyleSheet(
        "QMessageBox { background-color: #ffffff; }"
        "QMessageBox QLabel { color: #2c3e50; font-family: 'Segoe UI', sans-serif; font-size: 10pt; }"
        "QPushButton { background-color: #eef1f5; color: #2c3e50; border: 1px solid #d0d7e0;"
        "              border-radius: 8px; padding: 6px 18px; font-size: 10pt; }"
        "QPushButton:hover { background-color: #e0e7f0; }"
        );
    box.exec();
}

void MainWindow::onSimulationTimer()
{
    if (!m_isRunning)
        return;

    if (!evacuationModeActive) {
        int a1 = m_model->getFoodAmount();
        m_model->update();
        int a2 = m_model->getFoodAmount();
        if (a1 > a2)
            addLogMessage("Вибросигнал: Кормление");
    }
    m_tickCounter++;
}

void MainWindow::on_startButton_clicked()
{
    if (!m_isRunning && m_model->getLightBrightness() == 20) {
        m_isRunning = true;
        m_timer->start(1000);
        updateFeederDisplay();
        startLightCycle();
        addLogMessage("Симуляция запущена");
    }
    else if (!m_isRunning) {
        m_isRunning = true;
        m_timer->start(1000);
        lightTransitionActive = true;
        addLogMessage("Симуляция запущена");
    }
}

void MainWindow::on_pauseButton_clicked()
{
    if (m_isRunning) {
        m_isRunning = false;
        m_timer->stop();
        lightTransitionActive = false;
        addLogMessage("Симуляция приостановлена");
    }
}

void MainWindow::on_resetButton_clicked()
{
    if(m_isRunning) {
        m_isRunning = false;
        m_timer->stop();
    }

    if(animationTimer) {
        animationTimer->stop();
    }

    if(lightTransitionActive) {
        lightTransitionTimer->stop();
        lightTransitionActive = false;
    }

    evacuationModeActive = false;
    fishesInSafeZone = 0;

    if(evacuationCheckTimer) {
        evacuationCheckTimer->stop();
    }

    delete m_model;
    m_model = new AquariumModel(this);

    connect(m_model, &AquariumModel::temperatureChanged, this, [this](double v) {
        ui->tempLabel->setText(QString("%1 C").arg(v, 0, 'f', 1));
    });
    connect(m_model, &AquariumModel::waterLevelChanged, this, [this](int v) {
        ui->waterLabel->setText(QString("%1 %").arg(v));
    });
    connect(m_model, &AquariumModel::foodAmountChanged, this, [this](int v) {
        ui->foodLabel->setText(QString("%1 %").arg(v));
    });
    connect(m_model, &AquariumModel::lightBrightnessChanged, this, &MainWindow::updateLight);
    connect(m_model, &AquariumModel::filterFlowChanged, this, [this](int v) {
        ui->filterLabel->setText(QString("%1 %").arg(v));
    });
    connect(m_model, &AquariumModel::alarmTriggered, this, &MainWindow::onAlarm);
    connect(m_model, &AquariumModel::waterLevelChanged, this, &MainWindow::updateWaterLevel);

    updateUIFromModel();

    updateWaterLevel(m_model->getWaterLevel());
    updateLight(m_model->getLightBrightness());

    for(int i = 0; i < fishes.size(); i++) {
        fishes[i]->setVisible(true);
        fishes[i]->setPos(50 + i * 80, 100 + i * 50);
        fishSpeeds[i] = 2.0 + (rand() % 20) / 10.0;
        if(rand() % 2 == 0)
            fishSpeeds[i] = -fishSpeeds[i];
    }

    if(animationTimer) {
        animationTimer->start();
    }

    m_isRunning = false;
    m_tickCounter = 0;

    connect(m_model, &AquariumModel::foodAmountChanged, this, &MainWindow::updateFeederDisplay);
    updateFeederDisplay();

    addLogMessage("Система сброшена. Нажмите «Старт» для запуска симуляции");
}

void MainWindow::on_feedButton_clicked() {
    m_model->addFood();
    addLogMessage("Кормушка была наполнена");
}

void MainWindow::on_clogButton_clicked()
{
    m_model->clogFilter();
}

void MainWindow::on_unclogButton_clicked()
{
    m_model->unclogFilter();
}

void MainWindow::create_fishes(int count) {
    // Разные цвета рыбок
    QList<QColor> colors = {
        QColor(255, 160, 30),   // оранжевая
        QColor(255, 80, 80),    // красная
        QColor(80, 180, 255)    // синяя
    };
    for(int i = 0; i < count; i++) {
        QColor c = colors[i % colors.size()];
        QGraphicsEllipseItem* fish = aquariumScene->addEllipse(
            0, 0, 34, 16,
            QPen(c.darker(140), 1),
            QBrush(c));
        fish->setPos(80 + i * 180, 100 + i * 90);
        fish->setZValue(7);
        fishes.push_back(fish);
        fishSpeeds[i] = 2.0 + (rand() % 20) / 10.0;
    }
}

void MainWindow::moveFishes()
{
    int safeZoneX = 650;  // граница отсека

    for(int i = 0; i < fishes.size(); i++) {
        if(!fishes[i]->isVisible())
            continue;

        QPointF pos = fishes[i]->pos();
        pos.rx() += fishSpeeds[i];
        pos.ry() += 0.6*fishSpeeds[i];

        if(pos.x() >= safeZoneX && evacuationModeActive) {
            fishes[i]->setVisible(false);
            fishesInSafeZone++;
            addLogMessage("Рыба " + QString::number(i+1) + " заплыла в отсек (" +
                          QString::number(fishesInSafeZone) + "/" +
                          QString::number(fishes.size()) + ")");
            continue;
        }

        if(!evacuationModeActive) {
            if(pos.x() > 640 || pos.x() < 45 || pos.y() < 65 || pos.y() > 410) {
                fishSpeeds[i] = -fishSpeeds[i];
                pos.rx() += fishSpeeds[i];
            }
        } else {
            if(pos.x() > 655) {
                pos.setX(655);
            }
            if(pos.x() < 45) {
                fishSpeeds[i] = -fishSpeeds[i];
                pos.rx() += fishSpeeds[i];
            }
        }

        fishes[i]->setPos(pos);
        fishes[i]->setTransform(QTransform::fromScale(fishSpeeds[i] > 0 ? 1 : -1, 1));
    }
}
void MainWindow::updateWaterLevel(int level) {
    // Вода занимает level% от высоты зоны воды (420px до дна)
    int waterHeight = 420 * level / 100;
    waterItem->setRect(0, 420 - waterHeight, 686, waterHeight);
}

void MainWindow::updateLight(int brightness) {
    int opacity = 255 - (brightness * 255 / 100);
    darkOverlay->setBrush(QBrush(QColor(0, 0, 0, opacity)));

    if(brightness <= 20) {
        ui->lightLabel->setText("выключен");
    } else {
        ui->lightLabel->setText(QString("%1%").arg(brightness));
    }
}

void MainWindow::updateFeederDisplay()
{
    int fillHeight = 35 * m_model->getFoodAmount() / 100;
    int fillY = 15 + (35 - fillHeight);
    feederLevel->setRect(15, fillY, 55, fillHeight);

    if (m_model->getFoodAmount() != 100) {
        // Рыбы плывут в левый угол (к кормушке)
        for(int i = 0; i < fishes.size(); i++) {
            fishSpeeds[i] = -abs(fishSpeeds[i]);
            fishSpeeds[i] *= 1.5;  // ускорение
        }

        // Через 5 секунд вернуть нормальное поведение (если не режим эвакуации)
        QTimer::singleShot(5000, this, [this]() {
            if(!evacuationModeActive) {
                for(int i = 0; i < fishes.size(); i++) {
                    fishSpeeds[i] = abs(fishSpeeds[i]) * 0.7;
                    if(rand() % 2 == 0) fishSpeeds[i] = -fishSpeeds[i];
                }
            }
        });
    }
}

void MainWindow::on_evacuationButton_clicked()
{
    if(evacuationModeActive) {
        addLogMessage("Эвакуация уже запущена");
        return;
    }
    m_model->setLightBrightness(20);

    addLogMessage("Режим эвакуации: рыбы плывут в отсек");
    fishesInSafeZone = 0;
    evacuationModeActive = true;

    for(int i = 0; i < fishes.size(); i++) {
        fishes[i]->setVisible(true);
        fishSpeeds[i] = abs(fishSpeeds[i]);
        fishSpeeds[i] *= 1.5;
    }

    evacuationCheckTimer->start();
}

void MainWindow::on_finishCleaningButton_clicked()
{
    if(!evacuationModeActive) {
        addLogMessage("Эвакуация не запущена");
        return;
    }
    addLogMessage("Завершение уборки: наполнение водой...");

    m_model->setWaterLevel(85);

    for(int i = 0; i < fishes.size(); i++) {
        fishes[i]->setVisible(true);

        fishSpeeds[i] = 2.0 + (rand() % 20) / 10.0;
        if(rand() % 2 == 0)
            fishSpeeds[i] = -fishSpeeds[i];

        fishes[i]->setPos(640, 370 + i * 20);
    }

    evacuationModeActive = false;
    fishesInSafeZone = 0;

    if(evacuationCheckTimer) {
        evacuationCheckTimer->stop();
    }

    evacuationModeActive = false;
    if(lightTransitionActive) {
        lightTransitionTimer->stop();
        lightTransitionActive = false;
    }
    startLightCycle();
    addLogMessage("Уборка завершена. Рыбы возвращены в аквариум.");
}

void MainWindow::checkAllFishInSafeZone()
{
    if(!evacuationModeActive)
        return;

    if(fishesInSafeZone >= fishes.size()) {
        evacuationCheckTimer->stop();
        addLogMessage("Все рыбы в отсеке. Начинаем слив воды...");

        m_model->setWaterLevel(15);

        if(lightTransitionActive) {
            lightTransitionTimer->stop();
            lightTransitionActive = false;
        }
        m_model->setLightBrightness(100);
    }
}

void MainWindow::startLightCycle()
{
    m_model->setLightBrightness(20);

    QTimer::singleShot(5000, this, [this]() {
        startSunrise();
    });
}

void MainWindow::startSunrise()
{
    if(evacuationModeActive || !m_isRunning)
        return;

    addLogMessage("Рассвет: свет плавно включается");
    targetBrightness = 100;
    lightTransitionActive = true;
    lightStep = 2;
    lightTransitionTimer->start();
}

void MainWindow::startSunset()
{
    if(evacuationModeActive || !m_isRunning)
        return;

    addLogMessage("Закат: свет плавно выключается");
    targetBrightness = 20;
    lightTransitionActive = true;
    lightStep = -2;
    lightTransitionTimer->start();
}

void MainWindow::updateLightTransition()
{
    if (!m_isRunning || !lightTransitionActive || evacuationModeActive)
        return;

    int current = m_model->getLightBrightness();
    int next = current + lightStep;

    if((lightStep > 0 && next >= targetBrightness) ||
        (lightStep < 0 && next <= targetBrightness)) {
        // Достигли цели
        m_model->setLightBrightness(targetBrightness);
        lightTransitionTimer->stop();
        lightTransitionActive = false;

        if(targetBrightness == 100) {
            QTimer::singleShot(20000, this, &MainWindow::startSunset);
        } else if(targetBrightness == 20) {
            QTimer::singleShot(5000, this, &MainWindow::startSunrise);
        }
    } else {
        m_model->setLightBrightness(next);
    }
}
