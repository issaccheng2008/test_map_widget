#ifndef SEEDSELECTIONWINDOW_H
#define SEEDSELECTIONWINDOW_H

#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class SeedSelectionWindow : public QDialog
{
    Q_OBJECT
public:
    explicit SeedSelectionWindow(QWidget *parent = nullptr);

    [[nodiscard]] int seedCount() const;
    [[nodiscard]] bool hasAllocatedSeeds() const;
    [[nodiscard]] QVector<int> seedChannels() const;
    [[nodiscard]] QVector<QVector<int>> channelGrid() const;

signals:
    void seedsChanged();

private slots:
    void addSeed();
    void removeSelectedSeed();
    void handleChannelChanged(int index);
    void updateRemoveButtonState();

private:
    void refreshComboOptions();
    int lowestAvailableChannel() const;
    void updateAddButtonState();
    void reassignComboRowProperties();

    QTableWidget *m_table = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QVector<int> m_seedChannels;
};

#endif // SEEDSELECTIONWINDOW_H
