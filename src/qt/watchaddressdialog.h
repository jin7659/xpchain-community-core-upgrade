#ifndef WATCHADDRESSDIALOG_H
#define WATCHADDRESSDIALOG_H

#include <QDialog>
#include <QTableWidgetItem>

namespace Ui {
class WatchAddressDialog;
}

class WatchAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WatchAddressDialog(QWidget *parent = nullptr);
    ~WatchAddressDialog();

private Q_SLOTS:
    void on_addButton_clicked();
    void on_deleteButton_clicked();
    void on_closeButton_clicked();
    void on_addressTable_itemSelectionChanged();

private:
    Ui::WatchAddressDialog *ui;

    void refreshList();
    bool validateAddress(const QString& address);
};

#endif // WATCHADDRESSDIALOG_H
