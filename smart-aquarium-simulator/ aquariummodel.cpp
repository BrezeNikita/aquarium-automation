#include "AquariumModel.h"
#include <QRandomGenerator>

AquariumModel::AquariumModel(QObject *parent)
    : QObject(parent)
    , m_temperature(26.0)
    , m_waterLevel(85)
    , m_foodAmount(40)
    , m_lightBrightness(20)
    , m_filterFlow(100)
    , m_filterClogged(false)
{
}

double AquariumModel::getTemperature() const {
    return m_temperature;
}
int AquariumModel::getWaterLevel() const {
    return m_waterLevel;
}
int AquariumModel::getFoodAmount() const {
    return m_foodAmount;
}
int AquariumModel::getLightBrightness() const {
    return m_lightBrightness;
}
int AquariumModel::getFilterFlow() const {
    return m_filterFlow;
}
bool AquariumModel::isFilterClogged() const {
    return m_filterClogged;
}

void AquariumModel::setLightBrightness(int value)
{
    m_lightBrightness = qBound(0, value, 100);
    emit lightBrightnessChanged(m_lightBrightness);
}

void AquariumModel::addFood()
{
    m_foodAmount = 100;
    emit foodAmountChanged(m_foodAmount);
}

void AquariumModel::clogFilter()
{
    m_filterClogged = true;
    m_filterFlow = 15;
    emit filterFlowChanged(m_filterFlow);
    emit alarmTriggered("Фильтр засорился! Скорость потока: 15%");
}

void AquariumModel::unclogFilter()
{
    m_filterClogged = false;
    m_filterFlow = 100;
    emit filterFlowChanged(m_filterFlow);
    emit alarmTriggered("Фильтр очищен. Скорость потока восстановлена.");
}

void AquariumModel::update()
{
    // Естественные изменения параметров
    static int feedCounter = 0;
    if (feedCounter == 10) {
        m_foodAmount = qMax(0, m_foodAmount - 5);
        emit foodAmountChanged(m_foodAmount);
        feedCounter = 0;
        if (m_foodAmount == 20 || m_foodAmount == 5) {
            emit alarmTriggered("Наполните кормушку");
        }
    }
    feedCounter++;

    // Колебания температуры
    double delta = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.3;
    m_temperature = qBound(24.0, m_temperature + delta, 28.0);
    emit temperatureChanged(m_temperature);
}

void AquariumModel::setWaterLevel(int level)
{
    m_waterLevel = qBound(0, level, 100);
    emit waterLevelChanged(m_waterLevel);
}
