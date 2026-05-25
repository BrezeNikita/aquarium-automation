#ifndef AQUARIUMMODEL_H
#define AQUARIUMMODEL_H

#include <QObject>

class AquariumModel : public QObject
{
    Q_OBJECT
public:
    explicit AquariumModel(QObject* parent = nullptr);

    double getTemperature() const;
    int getWaterLevel() const;
    int getFoodAmount() const;
    int getLightBrightness() const;
    int getFilterFlow() const;
    bool isFilterClogged() const;

    void setLightBrightness(int value);
    void setWaterLevel(int level);
    void addFood();
    void clogFilter();
    void unclogFilter();

    void update();  // вызывается каждый такт времени

signals:
    void temperatureChanged(double value);
    void waterLevelChanged(int value);
    void foodAmountChanged(int value);
    void lightBrightnessChanged(int value);
    void filterFlowChanged(int value);
    void alarmTriggered(QString message);

private:
    double m_temperature;
    int m_waterLevel;
    int m_foodAmount;
    int m_lightBrightness;
    int m_filterFlow;
    bool m_filterClogged;
};

#endif
