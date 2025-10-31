#include "SeedSelectionWindow.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "generate_path.h"

namespace
{
constexpr int kNameColumn = 0;
constexpr int kChannelColumn = 1;
}

SeedSelectionWindow::SeedSelectionWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Seed Selection"));
    setModal(false);
    resize(400, 320);

    auto *layout = new QVBoxLayout(this);
    auto *gridLabel = new QLabel(tr("Grid size: %1 m").arg(grid_size), this);
    layout->addWidget(gridLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    QStringList headers{tr("Seed"), tr("Channel")};
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);

    auto *buttonRow = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_addButton = new QPushButton(tr("Add Seed"), buttonRow);
    m_removeButton = new QPushButton(tr("Remove Seed"), buttonRow);
    m_removeButton->setEnabled(false);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    layout->addWidget(buttonRow);

    connect(m_addButton, &QPushButton::clicked, this, &SeedSelectionWindow::addSeed);
    connect(m_removeButton, &QPushButton::clicked, this, &SeedSelectionWindow::removeSelectedSeed);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &SeedSelectionWindow::updateRemoveButtonState);

    updateAddButtonState();
}

int SeedSelectionWindow::seedCount() const
{
    return m_seedChannels.size();
}

bool SeedSelectionWindow::hasAllocatedSeeds() const
{
    return !m_seedChannels.isEmpty();
}

QVector<int> SeedSelectionWindow::seedChannels() const
{
    return m_seedChannels;
}

QVector<QVector<int>> SeedSelectionWindow::channelGrid() const
{
    QVector<QVector<int>> grid;
    if (!m_seedChannels.isEmpty())
        grid.append(m_seedChannels);
    return grid;
}

void SeedSelectionWindow::addSeed()
{
    const int maxSeeds = channel_number + 1; // include empty channel 0
    if (m_seedChannels.size() >= maxSeeds)
        return;

    const int row = m_seedChannels.size();
    m_table->insertRow(row);

    auto *nameItem = new QTableWidgetItem(tr("Seed %1").arg(row + 1));
    nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_table->setItem(row, kNameColumn, nameItem);

    auto *combo = new QComboBox(m_table);
    for (int channel = 0; channel <= channel_number; ++channel) {
        const QString label = channel == 0 ? tr("Channel 0 (Empty)") : tr("Channel %1").arg(channel);
        combo->addItem(label, channel);
    }
    combo->setProperty("row", row);
    m_table->setCellWidget(row, kChannelColumn, combo);

    m_seedChannels.append(-1);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SeedSelectionWindow::handleChannelChanged);

    const int defaultChannel = lowestAvailableChannel();
    const int effectiveChannel = defaultChannel >= 0 ? defaultChannel : 0;
    {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(combo->findData(effectiveChannel));
    }
    m_seedChannels[row] = effectiveChannel;

    refreshComboOptions();
    updateAddButtonState();
    emit seedsChanged();
}

void SeedSelectionWindow::removeSelectedSeed()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_seedChannels.size())
        return;

    QWidget *comboWidget = m_table->cellWidget(row, kChannelColumn);
    if (comboWidget)
        comboWidget->deleteLater();

    m_table->removeRow(row);
    m_seedChannels.remove(row);

    reassignComboRowProperties();
    refreshComboOptions();
    updateAddButtonState();
    updateRemoveButtonState();
    emit seedsChanged();
}

void SeedSelectionWindow::handleChannelChanged(int)
{
    auto *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;

    const int row = combo->property("row").toInt();
    if (row < 0 || row >= m_seedChannels.size())
        return;

    const int channel = combo->currentData().toInt();
    if (m_seedChannels[row] == channel)
        return;

    m_seedChannels[row] = channel;
    refreshComboOptions();
    updateAddButtonState();
    emit seedsChanged();
}

void SeedSelectionWindow::updateRemoveButtonState()
{
    const bool hasSelection = m_table->currentRow() >= 0;
    m_removeButton->setEnabled(hasSelection);
}

void SeedSelectionWindow::refreshComboOptions()
{
    QSet<int> usedChannels;
    for (int channel : m_seedChannels) {
        if (channel >= 0)
            usedChannels.insert(channel);
    }

    for (int row = 0; row < m_seedChannels.size(); ++row) {
        auto *combo = qobject_cast<QComboBox *>(m_table->cellWidget(row, kChannelColumn));
        if (!combo)
            continue;

        const int currentChannel = m_seedChannels.value(row, -1);
        auto *model = qobject_cast<QStandardItemModel *>(combo->model());
        if (!model)
            continue;

        for (int channel = 0; channel <= channel_number && channel < model->rowCount(); ++channel) {
            QStandardItem *item = model->item(channel);
            if (!item)
                continue;

            const bool shouldEnable = !usedChannels.contains(channel) || channel == currentChannel;
            Qt::ItemFlags flags = item->flags();
            if (shouldEnable)
                item->setFlags(flags | Qt::ItemIsEnabled);
            else
                item->setFlags(flags & ~Qt::ItemIsEnabled);
        }
    }
}

int SeedSelectionWindow::lowestAvailableChannel() const
{
    QSet<int> usedChannels;
    for (int channel : m_seedChannels) {
        if (channel >= 0)
            usedChannels.insert(channel);
    }

    for (int channel = 0; channel <= channel_number; ++channel) {
        if (!usedChannels.contains(channel))
            return channel;
    }
    return -1;
}

void SeedSelectionWindow::updateAddButtonState()
{
    const int maxSeeds = channel_number + 1;
    const bool canAdd = m_seedChannels.size() < maxSeeds;
    if (m_addButton)
        m_addButton->setEnabled(canAdd);
}

void SeedSelectionWindow::reassignComboRowProperties()
{
    for (int row = 0; row < m_seedChannels.size(); ++row) {
        auto *item = m_table->item(row, kNameColumn);
        if (item)
            item->setText(tr("Seed %1").arg(row + 1));

        auto *combo = qobject_cast<QComboBox *>(m_table->cellWidget(row, kChannelColumn));
        if (combo)
            combo->setProperty("row", row);
    }
}
