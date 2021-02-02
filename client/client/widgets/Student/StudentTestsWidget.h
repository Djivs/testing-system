#ifndef STUDENTTESTSWIDGET_H
#define STUDENTTESTSWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTableView>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QDebug>

class StudentTestsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StudentTestsWidget(QList <QStringList> list,  QWidget *parent = nullptr);
    QString getSelectedTest() const {
        if(!table->selectionModel()->hasSelection())
            return "";
        int row = table->selectionModel()->selectedRows()[0].row();
         return model->data(model->index(row, 0, QModelIndex())).toString();};
public:
    QPushButton *goBack;
    QPushButton *start;
private:
    QTableView *table;
    QStandardItemModel *model;
    const QStringList params = {"test name", "subject", "planned date"};

signals:

};

#endif // STUDENTTESTSWIDGET_H
