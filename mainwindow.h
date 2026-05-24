#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QVector>
#include "AquariumModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSimulationTimer();
    void onAlarm(QString message);
    void addLogMessage(const QString& message);
    void updateUIFromModel();

    // Кнопки
    void on_startButton_clicked();
    void on_pauseButton_clicked();
    void on_resetButton_clicked();
    void on_feedButton_clicked();
    void on_clogButton_clicked();
    void on_unclogButton_clicked();
    void on_evacuationButton_clicked();
    void on_finishCleaningButton_clicked();

    void create_fishes(int count);
    void moveFishes();
    void checkAllFishInSafeZone();

    void updateWaterLevel(int level);

    void updateLight(int brightness);
    void startLightCycle();
    void startSunrise();
    void startSunset();
    void updateLightTransition();

    void updateFeederDisplay();

private:
    Ui::MainWindow* ui;
    AquariumModel* m_model;
    QTimer* m_timer;
    bool m_isRunning;
    int m_tickCounter;

    QGraphicsScene* aquariumScene;
    QVector<QGraphicsEllipseItem*> fishes;
    QGraphicsRectItem* waterItem;      // визуализация уровня воды
    QGraphicsRectItem* darkOverlay;    // для яркости света
    QTimer* animationTimer;

    double fishSpeeds[3];              // скорость каждой рыбы
    int fishesInSafeZone;              // счётчик рыб в отсеке
    bool evacuationModeActive;         // активен ли режим эвакуации
    QTimer* evacuationCheckTimer;      // таймер для проверки, все ли рыбы в отсеке

    QTimer* lightTransitionTimer;      // таймер для проверки, достигнута ли нужная яркость
    int targetBrightness;              // процент яркости, которого  достичь
    bool lightTransitionActive;        // работает ли цикл смены дня и ночи
    int lightStep;                     // шаг, на который изменяется яркость

    QGraphicsRectItem* feeder;
    QGraphicsRectItem* feederLevel;    // визуализация содержимого кормушки
};

#endif