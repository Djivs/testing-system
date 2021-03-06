// Copyright 2021 Dmitriy Trifonov
#ifndef STUDENTRESULTSWIDGET_H
#define STUDENTRESULTSWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QPushButton>

class StudentResultsWidget : public QWidget {
    Q_OBJECT

 public:
    explicit StudentResultsWidget(QList <QStringList> new_list  = {}, QWidget *parent = nullptr);

 private:
    QPushButton *goBack;
    QTableView *table;
    QStandardItemModel *model;
    QList <QStringList> list;

 signals:
    void finished();
};

#endif  // STUDENTRESULTSWIDGET_H
